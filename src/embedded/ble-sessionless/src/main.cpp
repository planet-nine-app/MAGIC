/**
 * BLE Sessionless Target - UM Pro S3
 *
 * Physical MAGIC over BLE. Unlike the ESP-NOW wand/target pair (two boards,
 * one caster identity and one gateway identity), this is a single board
 * that owns ONE Sessionless identity and plays both caster and gateway
 * itself - self-registers with fount, and casts wandCast on its own behalf
 * whenever triggered. The trigger can come from the onboard BOOT button, or
 * from ANY generic BLE central (laptop, phone, Apple TV, a BLE explorer app
 * like LightBlue/nRF Connect, or ../../macos/BLEBridge/) writing any byte to
 * the characteristic below - no MAGIC/sessionless logic needs to exist on
 * the client platform at all, which is the whole point of this transport:
 * it's the one piece of hardware that can sit in BLE range of a laptop,
 * phone, AND an Apple TV (none of which need a custom MAGIC-aware app to
 * trigger it) rather than requiring native casting code on each of those
 * very different platforms.
 *
 * Reuses the exact wandCast spell (cost 50, mp: true) already deployed and
 * proven by ../espnow-wand/ and ../espnow-target/ against the hosted fount
 * gateway - no spellbook/backend changes needed to bring this transport up.
 * See ../espnow-wand/README.md for the wire-contract ground truth and the
 * debugging journey behind the WiFi/NTP/registration pieces reused here
 * near-verbatim.
 *
 * Hardware: Unexpected Maker Pro S3 (ESP32-S3)
 *
 * Onboard NeoPixel: GPIO18 data / GPIO17 power-enable, confirmed straight
 * from the official board definition shipped with the ESP32 Arduino core
 * (variants/um_pros3/pins_arduino.h - RGB_DATA=18, RGB_PWR=17). The original
 * version of this file used GPIO48 (a generic ESP32-S3-devkit NeoPixel pin)
 * which was never actually verified against this board's real wiring.
 *
 * Before building: copy config.h.example to config.h and fill in your WiFi
 * credentials (config.h is gitignored).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <sessionless.h>
#include "config.h"

// Same secp256k1-stack note as espnow-target: the vendored lib silently
// corrupts memory under the default 8KB loop task stack during key
// generation rather than failing cleanly (manifests as an "illegal
// argument: !secp256k1_fe_is_zero(&ge->x)" abort). Grow it before setup().
SET_LOOP_TASK_STACK_SIZE(32 * 1024);

// ============================================================================
// CONFIG
// ============================================================================

// BLE UUIDs - matching src/macos/BLEBridge/, which already scans for a
// device literally named "MAGIC-ProS3" advertising this service. Keep these
// in sync with BLEBridge.swift if either ever changes.
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BLE_DEVICE_NAME      "MAGIC-ProS3"

#define BUTTON_PIN 0 // BOOT button, active LOW (INPUT_PULLUP)
#define DEBOUNCE_MS 500

#define RGB_LED_PIN 18
#define RGB_LED_POWER_PIN 17 // must be driven HIGH to power the onboard NeoPixel
#define NUM_PIXELS 1

#define COLOR_NOT_READY_R 40
#define COLOR_NOT_READY_G 0
#define COLOR_NOT_READY_B 0

#define COLOR_ADVERTISING_R 0
#define COLOR_ADVERTISING_G 0
#define COLOR_ADVERTISING_B 150

#define COLOR_CONNECTED_R 0
#define COLOR_CONNECTED_G 150
#define COLOR_CONNECTED_B 0

#define COLOR_CASTING_R 128
#define COLOR_CASTING_G 0
#define COLOR_CASTING_B 128

#define CASTING_HOLD_MS 5000

#define NVS_NAMESPACE "ble_target"
#define NVS_PRIVATE_KEY "privKey"
#define NVS_PUBLIC_KEY "pubKey"
#define NVS_DEVICE_UUID "deviceUUID"

#define WIFI_CONNECT_TIMEOUT_MS 15000
#define NTP_SYNC_TIMEOUT_MS 15000

// Spell constants - baked in at compile time, same values as
// espnow-wand/espnow-target's espnow_protocol.h. Must match the wandCast
// spellbook entry in fount/src/server/node/spellbooks/spellbook.js
// (cost: 50, mp: true).
#define SPELL_NAME "wandCast"
#define SPELL_TOTAL_COST 50
#define SPELL_MP true

// ============================================================================
// STATE
// ============================================================================

Preferences preferences;
Adafruit_NeoPixel pixel(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

Keys deviceKeys;
char deviceUUID[37] = {0};
bool registered = false;
bool wifiConnected = false;
bool ntpSynced = false;

BLEServer *pServer = nullptr;
BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected = false;

volatile bool pendingCast = false;
unsigned long lastButtonPress = 0;

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

// Blue while advertising with nobody connected, green once a central is
// connected - either way "ready to be triggered", just communicates whether
// a BLE central is currently in the loop.
void setIdleColor() {
  if (deviceConnected) {
    setSolidColor(COLOR_CONNECTED_R, COLOR_CONNECTED_G, COLOR_CONNECTED_B);
  } else {
    setSolidColor(COLOR_ADVERTISING_R, COLOR_ADVERTISING_G, COLOR_ADVERTISING_B);
  }
}

// ============================================================================
// KEYS
// ============================================================================

void generateAndSaveKeys() {
  Serial.println("Generating new Sessionless keypair...");

  if (!sessionless::generateKeys(deviceKeys)) {
    Serial.println("Key generation FAILED!");
    while (1) { flashPixel(255, 0, 0); delay(500); }
  }

  preferences.putBytes(NVS_PRIVATE_KEY, deviceKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
  preferences.putBytes(NVS_PUBLIC_KEY, deviceKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);

  Serial.print("Public Key: ");
  printHex(deviceKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
  Serial.println();
}

void loadOrGenerateKeys() {
  size_t privLen = preferences.getBytesLength(NVS_PRIVATE_KEY);

  if (privLen == PRIVATE_KEY_SIZE_BYTES) {
    Serial.println("Loading existing keys from NVS...");
    preferences.getBytes(NVS_PRIVATE_KEY, deviceKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
    preferences.getBytes(NVS_PUBLIC_KEY, deviceKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.print("Public Key: ");
    printHex(deviceKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.println();
  } else {
    Serial.println("No keys found, generating new keypair...");
    generateAndSaveKeys();
  }

  size_t uuidLen = preferences.getBytesLength(NVS_DEVICE_UUID);
  if (uuidLen > 0 && uuidLen < sizeof(deviceUUID)) {
    preferences.getBytes(NVS_DEVICE_UUID, deviceUUID, uuidLen);
    deviceUUID[uuidLen] = '\0';
    registered = true;
    Serial.print("Already registered, deviceUUID: ");
    Serial.println(deviceUUID);
  } else {
    Serial.println("Not yet registered with fount.");
  }
}

// ============================================================================
// WIFI
// ============================================================================

uint64_t getRealEpochMs() {
  return (uint64_t)time(nullptr) * 1000ULL;
}

void syncNTPTime() {
  Serial.println("Syncing time via NTP...");
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  unsigned long start = millis();
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

    // Unlike ESP-NOW, BLE doesn't share a channel concept with WiFi STA, so
    // none of espnow-target's channel-pinning dance is needed here - just
    // keep the radio awake so HTTP calls to fount stay responsive.
    WiFi.setSleep(false);

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
    setIdleColor();
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

bool selfRegister() {
  char pubKeyHex[PUBLIC_KEY_SIZE_BYTES * 2 + 1];
  bytesToHex(deviceKeys.publicKey, PUBLIC_KEY_SIZE_BYTES, pubKeyHex);

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)getRealEpochMs());

  std::string message = std::string(timestampStr) + std::string(pubKeyHex);
  uint8_t signature[SIGNATURE_SIZE_BYTES];
  sessionless::sign(message, deviceKeys.privateKey, signature);

  char signatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(signature, SIGNATURE_SIZE_BYTES, signatureHex);

  WiFiClientSecure client;
  client.setInsecure(); // acceptable shortcut against a public HTTPS endpoint
                         // for this demo - do not reuse for anything
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

  StaticJsonDocument<512> response;
  DeserializationError err = deserializeJson(response, responseBody);
  if (err || status != 200 || !response.containsKey("uuid")) {
    Serial.println("Self-registration failed.");
    return false;
  }

  const char *uuid = response["uuid"];
  strncpy(deviceUUID, uuid, sizeof(deviceUUID) - 1);
  preferences.putBytes(NVS_DEVICE_UUID, deviceUUID, strlen(deviceUUID));
  registered = true;
  Serial.print("Registered! deviceUUID: ");
  Serial.println(deviceUUID);
  return true;
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

void notifyCentral(bool success) {
  if (!deviceConnected || pCharacteristic == nullptr) return;

  StaticJsonDocument<128> doc;
  doc["success"] = success;
  doc["spell"] = SPELL_NAME;
  doc["deviceUUID"] = deviceUUID;
  char payload[128];
  size_t len = serializeJson(doc, payload, sizeof(payload));

  pCharacteristic->setValue((uint8_t *)payload, len);
  pCharacteristic->notify();
}

// This board is its own caster AND its own (sole) gateway - there's no
// separate wand identity to relay for, so the spell casts with an empty
// gateways array, exactly like fount's resolve() already handles a spell
// with no gateway hops (`if (spell.gateways && spell.gateways.length > 0)`
// just skips the loop when it's empty).
void castSpell() {
  if (!registered) {
    Serial.println("Not registered with fount yet, dropping cast.");
    return;
  }

  unsigned long castTriggeredAt = millis();
  setSolidColor(COLOR_CASTING_R, COLOR_CASTING_G, COLOR_CASTING_B);
  notifyDemoServer(); // fire immediately, not gated on fount's response - see espnow-target for why

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)getRealEpochMs());
  uint32_t ordinal = (uint32_t)millis(); // just needs to vary, not be a real epoch

  // Must match fount's resolve() reconstruction exactly:
  // spell.timestamp + spell.spell + spell.casterUUID + spell.totalCost + spell.mp + spell.ordinal
  std::string message = std::string(timestampStr) + SPELL_NAME + std::string(deviceUUID) +
                         std::to_string(SPELL_TOTAL_COST) + (SPELL_MP ? "true" : "false") +
                         std::to_string(ordinal);

  uint8_t signature[SIGNATURE_SIZE_BYTES];
  sessionless::sign(message, deviceKeys.privateKey, signature);
  char signatureHex[SIGNATURE_SIZE_BYTES * 2 + 1];
  bytesToHex(signature, SIGNATURE_SIZE_BYTES, signatureHex);

  StaticJsonDocument<768> body;
  body["timestamp"] = timestampStr;
  body["spell"] = SPELL_NAME;
  body["casterUUID"] = deviceUUID;
  body["totalCost"] = SPELL_TOTAL_COST;
  body["mp"] = SPELL_MP;
  body["ordinal"] = ordinal;
  body["casterSignature"] = signatureHex;
  body.createNestedArray("gateways");

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

  Serial.println(success ? "Spell resolved successfully!" : "Spell fizzled.");
  notifyCentral(success);

  // Hold purple for a fixed CASTING_HOLD_MS window from the moment the cast
  // was triggered, same reasoning as espnow-target: keeps the visible
  // reaction in sync regardless of network latency.
  long elapsed = (long)(millis() - castTriggeredAt);
  if (elapsed < CASTING_HOLD_MS) {
    delay(CASTING_HOLD_MS - elapsed);
  }
  setIdleColor();
}

// ============================================================================
// BLE
// ============================================================================

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    deviceConnected = true;
    Serial.println("BLE central connected.");
    setIdleColor();
  }

  void onDisconnect(BLEServer *server) override {
    deviceConnected = false;
    Serial.println("BLE central disconnected - restarting advertising.");
    setIdleColor();
    BLEDevice::startAdvertising();
  }
};

// Any write at all - regardless of payload - is treated as a cast trigger.
// Deliberately generic: any BLE central (a phone/laptop/Apple TV app, or a
// bare "write a byte" tool like LightBlue/nRF Connect) can trigger a cast
// without needing to construct a MAGIC-shaped payload, since all the
// signing happens on this board.
class CastCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    Serial.println("BLE write received - triggering cast.");
    pendingCast = true;
  }
};

void initBLE() {
  Serial.println("\n=== Initializing BLE ===");

  BLEDevice::init(BLE_DEVICE_NAME);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ |
      BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_NOTIFY |
      BLECharacteristic::PROPERTY_INDICATE
  );
  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new CastCharacteristicCallbacks());
  pCharacteristic->setValue("ready");

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE initialized!");
  Serial.printf("Device name: %s\n", BLE_DEVICE_NAME);
  Serial.println("Waiting for a central to connect...");
}

// ============================================================================
// BUTTON
// ============================================================================

void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = now;
      Serial.println("BOOT pressed - triggering cast.");
      pendingCast = true;
    }
  }
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

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("\n================================");
  Serial.println("  BLE Sessionless Target - Pro S3");
  Serial.println("  Physical MAGIC: BLE-triggered wandCast");
  Serial.println("================================\n");

  preferences.begin(NVS_NAMESPACE, false);
  loadOrGenerateKeys();

  connectWiFi();

  if (wifiConnected && ntpSynced && !registered) {
    selfRegister();
  }

  initBLE(); // advertise regardless of WiFi state, so a central can at
             // least connect and see this board even before it's ready

  if (wifiConnected && ntpSynced) {
    setIdleColor();
  } else {
    setSolidColor(COLOR_NOT_READY_R, COLOR_NOT_READY_G, COLOR_NOT_READY_B);
  }

  Serial.println("\nReady. Press BOOT or write to the characteristic to cast.\n");
}

void loop() {
  maintainWiFi();
  checkButton();

  if (pendingCast) {
    pendingCast = false;
    if (wifiConnected && registered) {
      castSpell();
    } else if (!registered) {
      Serial.println("Cast triggered but not registered with fount yet - dropping.");
      flashPixel(255, 0, 0, 3);
    } else {
      Serial.println("Cast triggered but WiFi is down - dropping.");
      flashPixel(255, 0, 0, 3);
    }
  }

  if (wifiConnected && ntpSynced && !registered) {
    static unsigned long lastSelfRegisterAttempt = 0;
    if (millis() - lastSelfRegisterAttempt > 5000) {
      selfRegister();
      lastSelfRegisterAttempt = millis();
      if (registered) setIdleColor();
    }
  }

  delay(10);
}
