/**
 * ESP-NOW Target - UM Pro S3
 *
 * The gateway half of the physical-MAGIC wand demo. Listens for the wand's
 * ESP-NOW messages and does 100% of the WiFi/HTTP work: registers itself
 * with fount, relays the wand's own registration request, and relays the
 * wand's spell casts by adding its own gateway signature and POSTing the
 * full MAGIC payload to fount's /resolve endpoint.
 *
 * Hardware: Unexpected Maker Pro S3 (ESP32-S3)
 *
 * Onboard NeoPixel: GPIO18 data / GPIO17 power-enable, confirmed straight
 * from the official board definition shipped with the ESP32 Arduino core
 * (~/.platformio/packages/framework-arduinoespressif32/variants/um_pros3/
 * pins_arduino.h - RGB_DATA=18, RGB_PWR=17). Identical on the wand's
 * um_tinys3 variant, so both boards share this wiring.
 *
 * Before building: copy config.h.example to config.h and fill in your WiFi
 * credentials (config.h is gitignored).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <sessionless.h>
#include "espnow_protocol.h"
#include "config.h"

// The vendored secp256k1 library is stack-heavy (its own README warns about
// this on other embedded platforms) - the ESP32 Arduino core's default 8KB
// loop task stack isn't enough for secp256k1_ec_pubkey_create() during key
// generation. It doesn't fail cleanly: it silently corrupts memory, which
// manifests as an "illegal argument" abort deep inside secp256k1 claiming an
// all-zero pubkey. Grow the loop task stack before setup() runs.
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// ============================================================================
// CONFIG
// ============================================================================

// TEMPORARY diagnostic flag - see the #if DIAGNOSTIC_SKIP_WIFI block in
// setup() below. Set back to 0 once the WiFi-STA-concurrency question is
// resolved.
#define DIAGNOSTIC_SKIP_WIFI 0
#define WIFI_CHANNEL 11 // must match espnow-wand's WIFI_CHANNEL; only used by the diagnostic path above

#define RGB_LED_PIN 18
#define RGB_LED_POWER_PIN 17 // must be driven HIGH to power the onboard NeoPixel
#define NUM_PIXELS 1

// Solid green = listening for the wand. Solid purple = a spell was just
// received; held for a fixed duration regardless of how fount resolves it
// (a starting point - success/failure differentiation can layer on later).
#define COLOR_LISTENING_R 0
#define COLOR_LISTENING_G 150
#define COLOR_LISTENING_B 0

#define COLOR_CASTING_R 128
#define COLOR_CASTING_G 0
#define COLOR_CASTING_B 128

#define CASTING_HOLD_MS 5000

#define NVS_NAMESPACE "target"
#define NVS_PRIVATE_KEY "privKey"
#define NVS_PUBLIC_KEY "pubKey"
#define NVS_TARGET_UUID "targetUUID"

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define NTP_SYNC_TIMEOUT_MS 15000
#define TIME_SYNC_BROADCAST_INTERVAL_MS 5000

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
// STATE
// ============================================================================

Preferences preferences;
Adafruit_NeoPixel pixel(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

Keys targetKeys;
char targetUUID[37] = {0};
bool registered = false;
bool wifiConnected = false;
bool ntpSynced = false;

// ESP-NOW callbacks run outside loop(), so hand work off via these flags/
// buffers rather than doing HTTP calls directly inside the callback.
volatile bool pendingRegisterRelay = false;
RegisterRequest pendingRegisterReq;

volatile bool pendingCastRelay = false;
CastSpell pendingCast;

// ============================================================================
// HELPERS
// ============================================================================

void bytesToHex(const uint8_t *bytes, size_t len, char *out) {
  static const char hexChars[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hexChars[(bytes[i] >> 4) & 0xF];
    out[i * 2 + 1] = hexChars[bytes[i] & 0xF];
  }
  out[len * 2] = '\0';
}

void printHex(const uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (bytes[i] < 0x10) Serial.print('0');
    Serial.print(bytes[i], HEX);
  }
}

void flashPixel(uint8_t r, uint8_t g, uint8_t b, int times = 1) {
  for (int i = 0; i < times; i++) {
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
    delay(200);
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));
    pixel.show();
    if (i < times - 1) delay(150);
  }
}

void setSolidColor(uint8_t r, uint8_t g, uint8_t b) {
  pixel.setPixelColor(0, pixel.Color(r, g, b));
  pixel.show();
}

void setListeningColor() {
  setSolidColor(COLOR_LISTENING_R, COLOR_LISTENING_G, COLOR_LISTENING_B);
}

// ============================================================================
// KEYS
// ============================================================================

void generateAndSaveKeys() {
  Serial.println("Generating new Sessionless keypair...");

  if (!sessionless::generateKeys(targetKeys)) {
    Serial.println("Key generation FAILED!");
    while (1) { flashPixel(255, 0, 0); delay(500); }
  }

  preferences.putBytes(NVS_PRIVATE_KEY, targetKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
  preferences.putBytes(NVS_PUBLIC_KEY, targetKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);

  Serial.print("Public Key: ");
  printHex(targetKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
  Serial.println();
}

void loadOrGenerateKeys() {
  size_t privLen = preferences.getBytesLength(NVS_PRIVATE_KEY);

  if (privLen == PRIVATE_KEY_SIZE_BYTES) {
    Serial.println("Loading existing keys from NVS...");
    preferences.getBytes(NVS_PRIVATE_KEY, targetKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
    preferences.getBytes(NVS_PUBLIC_KEY, targetKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.print("Public Key: ");
    printHex(targetKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.println();
  } else {
    Serial.println("No keys found, generating new keypair...");
    generateAndSaveKeys();
  }

  size_t uuidLen = preferences.getBytesLength(NVS_TARGET_UUID);
  if (uuidLen > 0 && uuidLen < sizeof(targetUUID)) {
    preferences.getBytes(NVS_TARGET_UUID, targetUUID, uuidLen);
    targetUUID[uuidLen] = '\0';
    registered = true;
    Serial.print("Already registered, targetUUID: ");
    Serial.println(targetUUID);
  } else {
    Serial.println("Not yet registered with fount.");
  }
}

// ============================================================================
// WIFI
// ============================================================================

// fount's global timestamp-freshness middleware needs a real wall-clock
// value, not millis()-since-boot. Once synced, ESP32's SNTP client keeps
// time(nullptr) accurate in the background on its own, no manual offset
// tracking needed on the target's side (unlike the wand, which has no NTP
// of its own - see MSG_TIME_SYNC below).
uint64_t getRealEpochMs() {
  return (uint64_t)time(nullptr) * 1000ULL;
}

void syncNTPTime() {
  Serial.println("Syncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  unsigned long start = millis();
  // A plausible "real" epoch (year 2020+) - anything below this means NTP
  // hasn't actually landed a response yet, just still ticking from 1970.
  const time_t plausibleEpoch = 1577836800; // 2020-01-01
  while (time(nullptr) < plausibleEpoch && millis() - start < NTP_SYNC_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }

  if (time(nullptr) >= plausibleEpoch) {
    ntpSynced = true;
    Serial.printf("\nNTP synced. Epoch: %llu ms\n", (unsigned long long)getRealEpochMs());
  } else {
    ntpSynced = false;
    Serial.println("\nNTP sync failed - will keep retrying in loop().");
  }
}

void connectWiFi() {
  Serial.printf("Connecting to WiFi \"%s\"...\n", WIFI_SSID);
  // Was WIFI_AP_STA on the theory that keeping the AP interface active
  // "keeps ESP-NOW happy on some cores" - that was never actually verified
  // on real hardware (this whole firmware was written before any board was
  // in hand). AP_STA creates a second interface with its own MAC; worth
  // testing whether that's actually interfering with ESP-NOW receive rather
  // than helping it, now that channel mismatch has been ruled out as the
  // cause of the target seeing zero packets from the wand.
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.printf("WiFi channel: %d\n", WiFi.channel());

    // WiFi modem sleep is on by default once associated with an AP - its
    // wake schedule is synced to the AP's beacon interval, not to ESP-NOW
    // traffic from an unrelated peer (the wand). That can make inbound
    // ESP-NOW broadcasts from the wand nearly impossible to catch even on
    // the correct channel, while the target's own outgoing broadcasts (and
    // the wand's reception of them, since the wand never sleeps - it's
    // never associated with any AP) keep working fine. Well-documented
    // ESP-NOW-plus-active-STA gotcha; disable it.
    WiFi.setSleep(false);

    // We've only ever relied on ESP-NOW implicitly following whatever
    // channel WiFi.begin() associated on - never verified that ESP-NOW's
    // own internal channel state actually tracks that automatically on
    // this core version, rather than needing an explicit nudge. Cheap to
    // rule out: re-assert the channel we just observed ourselves connecting
    // on, explicitly, via the same API the wand uses to pin its own
    // channel - redundant if implicit tracking already works, but isolates
    // whether it doesn't.
    int actualChannel = WiFi.channel();
    esp_wifi_set_channel(actualChannel, WIFI_SECOND_CHAN_NONE);
    Serial.printf("Explicitly re-pinned ESP-NOW channel to %d (matching STA).\n", actualChannel);

    syncNTPTime();
  } else {
    wifiConnected = false;
    Serial.println("\nWiFi connection failed - will keep retrying in loop().");
  }
}

void maintainWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    if (wifiConnected) {
      Serial.println("WiFi connection lost!");
      wifiConnected = false;
    }
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 5000) {
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      lastAttempt = millis();
    }
  } else if (!wifiConnected) {
    wifiConnected = true;
    Serial.println("WiFi reconnected!");
    if (!ntpSynced) syncNTPTime();
    setListeningColor();
  }

  if (wifiConnected && !ntpSynced) {
    static unsigned long lastNtpAttempt = 0;
    if (millis() - lastNtpAttempt > 5000) {
      syncNTPTime();
      lastNtpAttempt = millis();
    }
  }
}

// ============================================================================
// FOUNT HTTP
// ============================================================================

// Shared by both self-registration and relaying the wand's registration.
bool registerWithFount(const char *pubKeyHex, const char *timestampStr,
                        const char *signatureHex, JsonDocument &responseOut) {
  WiFiClientSecure client;
  client.setInsecure(); // acceptable shortcut against a public HTTPS endpoint
                         // for this weekend demo - do not reuse for anything
                         // security-sensitive.

  HTTPClient http;
  String url = String(GATEWAY_BASE) + "/user/create";

  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed (register)");
    return false;
  }
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<384> body;
  body["timestamp"] = timestampStr;
  body["pubKey"] = pubKeyHex;
  body["signature"] = signatureHex;
  String payload;
  serializeJson(body, payload);

  int status = http.PUT(payload); // fount registers this route as app.put, not app.post
  String responseBody = http.getString();
  http.end();

  Serial.printf("PUT /user/create -> HTTP %d\n", status);

  DeserializationError err = deserializeJson(responseOut, responseBody);
  if (err) {
    Serial.print("Failed to parse registration response: ");
    Serial.println(err.c_str());
    return false;
  }

  return status == 200 && responseOut.containsKey("uuid");
}

bool selfRegister() {
  char pubKeyHex[PUBLIC_KEY_SIZE_BYTES * 2 + 1];
  bytesToHex(targetKeys.publicKey, PUBLIC_KEY_SIZE_BYTES, pubKeyHex);

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)getRealEpochMs());

  std::string message = std::string(timestampStr) + std::string(pubKeyHex);
  uint8_t signature[SIGNATURE_SIZE_BYTES];
  sessionless::sign(message, targetKeys.privateKey, signature);

  char signatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(signature, SIGNATURE_SIZE_BYTES, signatureHex);

  StaticJsonDocument<512> response;
  bool ok = registerWithFount(pubKeyHex, timestampStr, signatureHex, response);

  if (ok) {
    const char *uuid = response["uuid"];
    strncpy(targetUUID, uuid, sizeof(targetUUID) - 1);
    preferences.putBytes(NVS_TARGET_UUID, targetUUID, strlen(targetUUID));
    registered = true;
    Serial.print("Target registered! uuid: ");
    Serial.println(targetUUID);
    return true;
  }

  Serial.println("Target self-registration failed.");
  return false;
}

// Relays the wand's own already-signed registration request verbatim -
// the target never sees the wand's private key, it's just a dumb relay here.
void relayWandRegistration(const RegisterRequest &req) {
  char pubKeyHex[PUBLIC_KEY_SIZE_BYTES * 2 + 1];
  bytesToHex(req.pubKey, PUBLIC_KEY_SIZE_BYTES, pubKeyHex);

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)req.timestamp);

  char signatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(req.signature, SIGNATURE_SIZE_BYTES, signatureHex);

  Serial.println("Relaying wand registration to fount...");

  StaticJsonDocument<512> response;
  bool ok = registerWithFount(pubKeyHex, timestampStr, signatureHex, response);

  RegisterResponse resp;
  resp.msgType = MSG_REGISTER_RESPONSE;

  if (ok) {
    const char *uuid = response["uuid"];
    resp.success = 1;
    strncpy(resp.casterUUID, uuid, sizeof(resp.casterUUID) - 1);
    resp.casterUUID[sizeof(resp.casterUUID) - 1] = '\0';
    Serial.print("Wand registered! uuid: ");
    Serial.println(resp.casterUUID);
  } else {
    resp.success = 0;
    resp.casterUUID[0] = '\0';
    Serial.println("Wand registration failed.");
  }

  esp_err_t sendResult = esp_now_send(BROADCAST_MAC, (uint8_t *)&resp, sizeof(resp));
  if (sendResult != ESP_OK) {
    Serial.printf("esp_now_send (register response) failed: %s\n", esp_err_to_name(sendResult));
  }
}

// Best-effort - the big-screen display is a demo nicety, not part of the
// MAGIC protocol itself. Never let it block or fail the actual spell flow.
void notifyDemoServer() {
  HTTPClient http;
  if (http.begin(DEMO_SERVER_URL)) {
    http.addHeader("Content-Type", "application/json");
    int status = http.POST("{}");
    http.end();
    Serial.printf("Demo server notified -> HTTP %d\n", status);
  } else {
    Serial.println("Demo server HTTP begin failed (is it running and DEMO_SERVER_URL correct in config.h?)");
  }
}

void relayCastSpell(const CastSpell &cast) {
  if (!registered) {
    Serial.println("Target isn't registered with fount yet, dropping cast.");
    return;
  }

  unsigned long castReceivedAt = millis();
  setSolidColor(COLOR_CASTING_R, COLOR_CASTING_G, COLOR_CASTING_B);
  // Fire immediately on receipt, not gated on fount's response - the actual
  // resolve round trip (network + Netlify) can take long enough to land
  // suspiciously close to the green transition, which reads as "waiting for
  // green" even though structurally it wasn't. This keeps the big-screen
  // reaction perfectly in sync with the physical purple LED regardless of
  // backend latency, at the cost of celebrating fizzled casts too.
  notifyDemoServer();

  char casterTimestampStr[21];
  snprintf(casterTimestampStr, sizeof(casterTimestampStr), "%llu", (unsigned long long)cast.timestamp);
  char casterSignatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(cast.casterSignature, SIGNATURE_SIZE_BYTES, casterSignatureHex);

  uint64_t gatewayTimestamp = getRealEpochMs();
  uint32_t gatewayOrdinal = (uint32_t)millis(); // just needs to vary, not be a real epoch

  char gatewayTimestampStr[21];
  snprintf(gatewayTimestampStr, sizeof(gatewayTimestampStr), "%llu", (unsigned long long)gatewayTimestamp);
  char gatewayOrdinalStr[11];
  snprintf(gatewayOrdinalStr, sizeof(gatewayOrdinalStr), "%u", gatewayOrdinal);

  // Must match fount's resolve() reconstruction exactly:
  // gateway.timestamp + gateway.uuid + gateway.minimumCost + gateway.ordinal
  std::string gatewayMessage = std::string(gatewayTimestampStr) + std::string(targetUUID) +
                                std::to_string(GATEWAY_MINIMUM_COST) + std::string(gatewayOrdinalStr);

  uint8_t gatewaySignature[SIGNATURE_SIZE_BYTES];
  sessionless::sign(gatewayMessage, targetKeys.privateKey, gatewaySignature);
  char gatewaySignatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(gatewaySignature, SIGNATURE_SIZE_BYTES, gatewaySignatureHex);

  StaticJsonDocument<1024> body;
  body["timestamp"] = casterTimestampStr;
  body["spell"] = SPELL_NAME;
  body["casterUUID"] = cast.casterUUID;
  body["totalCost"] = SPELL_TOTAL_COST;
  body["mp"] = SPELL_MP;
  body["ordinal"] = cast.ordinal;
  body["casterSignature"] = casterSignatureHex;

  JsonArray gateways = body.createNestedArray("gateways");
  JsonObject gw = gateways.createNestedObject();
  gw["timestamp"] = gatewayTimestampStr;
  gw["uuid"] = targetUUID;
  gw["minimumCost"] = GATEWAY_MINIMUM_COST;
  gw["ordinal"] = gatewayOrdinal;
  gw["signature"] = gatewaySignatureHex;

  String payload;
  serializeJson(body, payload);

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(GATEWAY_BASE) + "/resolve/" + SPELL_NAME;

  bool success = false;

  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    int status = http.POST(payload);
    String responseBody = http.getString();
    http.end();

    Serial.printf("POST /resolve/%s -> HTTP %d\n", SPELL_NAME, status);
    Serial.println(responseBody);

    StaticJsonDocument<256> response;
    if (!deserializeJson(response, responseBody)) {
      success = status == 200 && response["success"] == true;
    }
  } else {
    Serial.println("HTTP begin failed (resolve)");
  }

  if (success) {
    Serial.println("Spell resolved successfully!");
  } else {
    Serial.println("Spell fizzled.");
  }

  // Hold purple for a fixed CASTING_HOLD_MS window from the moment the cast
  // was received, not just from when the HTTP call finished - the network
  // round trip (typically well under a second) eats into this window rather
  // than extending it.
  long elapsed = (long)(millis() - castReceivedAt);
  if (elapsed < CASTING_HOLD_MS) {
    delay(CASTING_HOLD_MS - elapsed);
  }
  setListeningColor();

  CastResult result;
  result.msgType = MSG_CAST_RESULT;
  result.success = success ? 1 : 0;
  esp_err_t sendResult = esp_now_send(BROADCAST_MAC, (uint8_t *)&result, sizeof(result));
  if (sendResult != ESP_OK) {
    Serial.printf("esp_now_send (cast result) failed: %s\n", esp_err_to_name(sendResult));
  }
}

// ============================================================================
// ESP-NOW
// ============================================================================

// framework-arduinoespressif32 3.x (IDF4-based, what this PlatformIO
// espressif32 platform version resolves) uses the older esp_now_recv_cb_t
// signature (mac, not esp_now_recv_info_t*). If you upgrade the platform to
// a newer arduino-esp32 core, this will need to change to
// (const esp_now_recv_info_t *info, const uint8_t *data, int len).
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < 1) return;
  uint8_t msgType = data[0];

  Serial.printf("ESP-NOW recv: msgType=%d len=%d from %02X:%02X:%02X:%02X:%02X:%02X\n",
                msgType, len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  if (msgType == MSG_REGISTER_REQUEST && len == sizeof(RegisterRequest)) {
    memcpy((void *)&pendingRegisterReq, data, sizeof(RegisterRequest));
    pendingRegisterRelay = true;
  } else if (msgType == MSG_CAST_SPELL && len == sizeof(CastSpell)) {
    memcpy((void *)&pendingCast, data, sizeof(CastSpell));
    pendingCastRelay = true;
  }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  // Quiet - see wand firmware for the same note.
}

void initEspNow() {
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1) { flashPixel(255, 0, 0); delay(500); }
  }

  if (esp_now_register_recv_cb(onDataRecv) != ESP_OK) {
    Serial.println("esp_now_register_recv_cb FAILED - target will never see incoming packets!");
  }
  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW broadcast peer!");
  }

  Serial.println("ESP-NOW ready (broadcast).");
}

// ============================================================================
// SETUP / LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(RGB_LED_POWER_PIN, OUTPUT);
  digitalWrite(RGB_LED_POWER_PIN, HIGH);
  pixel.begin();
  pixel.setPixelColor(0, pixel.Color(128, 0, 128)); // purple = booting
  pixel.show();

  Serial.println("\n================================");
  Serial.println("  ESP-NOW Target - Pro S3");
  Serial.println("  Physical MAGIC: gateway for wandCast");
  Serial.println("================================\n");

  preferences.begin(NVS_NAMESPACE, false);
  loadOrGenerateKeys();

#if DIAGNOSTIC_SKIP_WIFI
  // TEMPORARY: isolating whether ESP-NOW reception works at all on this
  // board when it's not dealing with an active WiFi STA association -
  // skips connectWiFi() entirely. wifiConnected stays false, so
  // registration/casting won't actually complete, but the "ESP-NOW recv:"
  // catch-all log in onDataRecv doesn't care about that - it'll fire on
  // ANY packet from the wand regardless. Flip DIAGNOSTIC_SKIP_WIFI back to
  // 0 once this test is done.
  //
  // connectWiFi() would normally put us on the AP's channel implicitly -
  // skipping it means nothing sets a channel at all, so we'd default to
  // whatever the radio boots to (not necessarily 11) and this test would
  // fail for the wrong reason (channel mismatch, not the real question).
  // Pin to WIFI_CHANNEL explicitly, same promiscuous-toggle approach the
  // wand uses for a disconnected STA.
  Serial.println("DIAGNOSTIC_SKIP_WIFI is on - not connecting to WiFi at all.");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  Serial.printf("Pinned to channel %d (no AP association).\n", WIFI_CHANNEL);
  initEspNow();
#else
  connectWiFi();
  initEspNow(); // ESP-NOW works alongside an active WiFi STA connection
#endif

  if (wifiConnected && ntpSynced && !registered) {
    selfRegister();
  }

  if (wifiConnected && ntpSynced) {
    setListeningColor();
  } else {
    setSolidColor(40, 0, 0); // dim red = not ready (WiFi and/or NTP not up yet)
  }

  Serial.println("\nReady. Listening for the wand...\n");
}

void loop() {
#if !DIAGNOSTIC_SKIP_WIFI
  maintainWiFi();
#endif

  if (pendingRegisterRelay) {
    pendingRegisterRelay = false;
    if (wifiConnected) {
      relayWandRegistration(pendingRegisterReq);
    } else {
      Serial.println("Wand registration pending but WiFi is down.");
    }
  }

  if (pendingCastRelay) {
    pendingCastRelay = false;
    if (wifiConnected) {
      relayCastSpell(pendingCast);
    } else {
      Serial.println("Cast received but WiFi is down - dropping.");
      flashPixel(255, 0, 0, 3);
    }
  }

  if (wifiConnected && ntpSynced && !registered) {
    static unsigned long lastSelfRegisterAttempt = 0;
    if (millis() - lastSelfRegisterAttempt > 5000) {
      selfRegister();
      lastSelfRegisterAttempt = millis();
      if (registered) setListeningColor();
    }
  }

  // Broadcast real epoch time for the wand to calibrate against - it has no
  // WiFi/NTP of its own (see MSG_TIME_SYNC in espnow_protocol.h).
  if (ntpSynced) {
    static unsigned long lastTimeSyncBroadcast = 0;
    if (millis() - lastTimeSyncBroadcast > TIME_SYNC_BROADCAST_INTERVAL_MS) {
      TimeSync sync;
      sync.msgType = MSG_TIME_SYNC;
      sync.epochMs = getRealEpochMs();
      esp_err_t sendResult = esp_now_send(BROADCAST_MAC, (uint8_t *)&sync, sizeof(sync));
      if (sendResult != ESP_OK) {
        Serial.printf("esp_now_send (time sync) failed: %s\n", esp_err_to_name(sendResult));
      }
      lastTimeSyncBroadcast = millis();
    }
  }

  delay(10);
}
