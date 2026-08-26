/**
 * ESP-NOW Wand - UM TinyS3
 *
 * The caster half of the physical-MAGIC wand demo. Speaks ESP-NOW only -
 * this firmware has no WiFi code at all. It registers itself with fount
 * (via the target relaying the request) and casts the "wandCast" spell on
 * every button press, both over ESP-NOW broadcast.
 *
 * Hardware: Unexpected Maker TinyS3 (ESP32-S3)
 *
 * Onboard NeoPixel: GPIO18 data / GPIO17 power-enable, confirmed straight
 * from the official board definition shipped with the ESP32 Arduino core
 * (~/.platformio/packages/framework-arduinoespressif32/variants/um_tinys3/
 * pins_arduino.h - RGB_DATA=18, RGB_PWR=17). Identical on the target's
 * um_pros3 variant, so both boards share this wiring.
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <sessionless.h>
#include "espnow_protocol.h"

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

#define RGB_LED_PIN 18
#define RGB_LED_POWER_PIN 17 // must be driven HIGH to power the onboard NeoPixel
#define NUM_PIXELS 1

// TinyS3[D] has both an onboard chip antenna and a u.FL connector for an
// external one, switchable via this pin (LOW = onboard/default, HIGH =
// external). This specific unit's onboard antenna turned out to be the
// actual root cause of every wand->target ESP-NOW failure this whole
// session - switching to the external path, even completely unterminated,
// fixed reception completely (confirmed via a minimal isolated test before
// applying it here). Physically attaching an external antenna should only
// improve on this further.
#define RF_SWITCH 38

#define BUTTON_PIN 0 // BOOT button
#define DEBOUNCE_MS 200

#define NVS_NAMESPACE "wand"
#define NVS_PRIVATE_KEY "privKey"
#define NVS_PUBLIC_KEY "pubKey"
#define NVS_ORDINAL "ordinal"
#define NVS_TARGET_CACHE "targets"
#define NVS_TARGET_COUNT "targetCount"

#define REGISTER_RETRY_MS 3000

// The target stays associated with whatever WiFi AP its venue has, so
// ESP-NOW between the two boards only works if the wand's radio is on that
// same channel - and a wand that travels between venues (BASEs, in agora
// terms) can't assume any single channel. Hops through 1-13, dwelling
// briefly on each, until it hears a target's MSG_TIME_SYNC broadcast; locks
// onto that channel, and resumes scanning if it goes stale (see
// TIME_SYNC_STALE_MS) - covers both "found a new venue" and "this venue's
// target rebooted onto a different channel." The onDataRecv TIME_SYNC
// handler re-asserts the channel authoritatively as its last action, which
// closes a real race against this scanner's hop timer (they run on
// different FreeRTOS tasks) in practice.
#define WIFI_MIN_CHANNEL 1
#define WIFI_MAX_CHANNEL 13
#define CHANNEL_DWELL_MS 400
#define TIME_SYNC_STALE_MS 20000

// A casterUUID is assigned per-backend (each target relays to its own
// venue's allyabase/basin instance), so the same wand keypair can hold a
// different UUID at every venue it's ever visited. Cached locally, keyed by
// the target's MAC (a stable proxy for "which venue/backend"), rather than
// as a single global fact - registration re-runs automatically whenever the
// wand meets a target MAC it has no cached UUID for yet.
#define MAX_TARGETS 8

static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ============================================================================
// STATE
// ============================================================================

Preferences preferences;
Adafruit_NeoPixel pixel(NUM_PIXELS, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

Keys wandKeys;
uint32_t castOrdinal = 0;

struct __attribute__((packed)) TargetEntry {
  uint8_t mac[6];
  char casterUUID[37];
};

TargetEntry targetCache[MAX_TARGETS];
int targetCacheCount = 0;

uint8_t lastTargetMac[6] = {0};
bool haveLastTargetMac = false;

unsigned long lastButtonPress = 0;
unsigned long lastRegisterAttempt = 0;

uint8_t currentScanChannel = WIFI_MIN_CHANNEL;
unsigned long lastChannelHop = 0;
unsigned long lastTimeSyncReceived = 0;

// The wand has no WiFi/NTP of its own, so it can't produce a real epoch
// timestamp on its own - fount rejects any request whose signed timestamp
// is more than 5 minutes from its own wall-clock time (a global check, not
// just on casting). The target syncs via NTP and broadcasts its real epoch
// periodically (MSG_TIME_SYNC); the wand calibrates off the most recent one
// it heard and extrapolates forward using its own millis() in between.
bool timeSynced = false;
uint64_t estimatedEpochMs = 0;
unsigned long millisAtSync = 0;

uint64_t getRealEpochMs() {
  return estimatedEpochMs + (millis() - millisAtSync);
}

bool macsEqual(const uint8_t *a, const uint8_t *b) {
  return memcmp(a, b, 6) == 0;
}

// Returns nullptr if this target's MAC has never registered with us before.
char *findCasterUUID(const uint8_t *mac) {
  for (int i = 0; i < targetCacheCount; i++) {
    if (macsEqual(targetCache[i].mac, mac)) return targetCache[i].casterUUID;
  }
  return nullptr;
}

void saveCasterUUID(const uint8_t *mac, const char *uuid) {
  int slot = -1;
  for (int i = 0; i < targetCacheCount; i++) {
    if (macsEqual(targetCache[i].mac, mac)) { slot = i; break; }
  }

  if (slot == -1) {
    if (targetCacheCount < MAX_TARGETS) {
      slot = targetCacheCount++;
    } else {
      // Cache full - evict the oldest entry (index 0) to make room.
      memmove(&targetCache[0], &targetCache[1], sizeof(TargetEntry) * (MAX_TARGETS - 1));
      slot = MAX_TARGETS - 1;
    }
    memcpy(targetCache[slot].mac, mac, 6);
  }

  strncpy(targetCache[slot].casterUUID, uuid, sizeof(targetCache[slot].casterUUID) - 1);
  targetCache[slot].casterUUID[sizeof(targetCache[slot].casterUUID) - 1] = '\0';

  preferences.putBytes(NVS_TARGET_CACHE, targetCache, sizeof(targetCache));
  preferences.putUInt(NVS_TARGET_COUNT, targetCacheCount);
}

void loadTargetCache() {
  targetCacheCount = preferences.getUInt(NVS_TARGET_COUNT, 0);
  if (targetCacheCount > MAX_TARGETS) targetCacheCount = MAX_TARGETS;

  size_t len = preferences.getBytesLength(NVS_TARGET_CACHE);
  if (len == sizeof(targetCache)) {
    preferences.getBytes(NVS_TARGET_CACHE, targetCache, sizeof(targetCache));
    Serial.printf("Loaded %d cached target registration(s) from NVS.\n", targetCacheCount);
  } else {
    targetCacheCount = 0;
  }
}

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
    delay(150);
    pixel.setPixelColor(0, pixel.Color(0, 0, 0));
    pixel.show();
    if (i < times - 1) delay(150);
  }
}

// ============================================================================
// KEYS
// ============================================================================

void generateAndSaveKeys() {
  Serial.println("Generating new Sessionless keypair...");

  if (!sessionless::generateKeys(wandKeys)) {
    Serial.println("Key generation FAILED!");
    while (1) { flashPixel(255, 0, 0); delay(500); }
  }

  preferences.putBytes(NVS_PRIVATE_KEY, wandKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
  preferences.putBytes(NVS_PUBLIC_KEY, wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);

  Serial.print("Public Key: ");
  printHex(wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
  Serial.println();
}

void loadOrGenerateKeys() {
  size_t privLen = preferences.getBytesLength(NVS_PRIVATE_KEY);

  if (privLen == PRIVATE_KEY_SIZE_BYTES) {
    Serial.println("Loading existing keys from NVS...");
    preferences.getBytes(NVS_PRIVATE_KEY, wandKeys.privateKey, PRIVATE_KEY_SIZE_BYTES);
    preferences.getBytes(NVS_PUBLIC_KEY, wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.print("Public Key: ");
    printHex(wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
    Serial.println();
  } else {
    Serial.println("No keys found, generating new keypair...");
    generateAndSaveKeys();
  }

  loadTargetCache();

  castOrdinal = preferences.getUInt(NVS_ORDINAL, 0);
}

// ============================================================================
// ESP-NOW
// ============================================================================

void sendRegisterRequest() {
  Serial.println("Sending registration request over ESP-NOW...");

  RegisterRequest req;
  req.msgType = MSG_REGISTER_REQUEST;
  memcpy(req.pubKey, wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES);
  req.timestamp = getRealEpochMs();

  char pubKeyHex[PUBLIC_KEY_SIZE_BYTES * 2 + 1];
  bytesToHex(wandKeys.publicKey, PUBLIC_KEY_SIZE_BYTES, pubKeyHex);

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)req.timestamp);

  std::string message = std::string(timestampStr) + std::string(pubKeyHex);
  sessionless::sign(message, wandKeys.privateKey, req.signature);

  esp_err_t sendResult = esp_now_send(BROADCAST_MAC, (uint8_t *)&req, sizeof(req));
  if (sendResult != ESP_OK) {
    Serial.printf("esp_now_send (register) failed: %s\n", esp_err_to_name(sendResult));
  }
}

void sendCastSpell(const char *targetCasterUUID) {
  Serial.println("Casting wandCast spell over ESP-NOW...");

  CastSpell cast;
  cast.msgType = MSG_CAST_SPELL;
  memcpy(cast.casterUUID, targetCasterUUID, sizeof(cast.casterUUID));
  cast.timestamp = getRealEpochMs();
  cast.ordinal = ++castOrdinal;
  preferences.putUInt(NVS_ORDINAL, castOrdinal);

  char timestampStr[21];
  snprintf(timestampStr, sizeof(timestampStr), "%llu", (unsigned long long)cast.timestamp);
  char ordinalStr[11];
  snprintf(ordinalStr, sizeof(ordinalStr), "%u", cast.ordinal);

  // Must match fount's resolve() reconstruction exactly:
  // spell.timestamp + spell.spell + spell.casterUUID + spell.totalCost + spell.mp + spell.ordinal
  std::string message = std::string(timestampStr) + SPELL_NAME + std::string(targetCasterUUID) +
                         std::to_string(SPELL_TOTAL_COST) + (SPELL_MP ? "true" : "false") + std::string(ordinalStr);

  sessionless::sign(message, wandKeys.privateKey, cast.casterSignature);

  esp_err_t sendResult = esp_now_send(BROADCAST_MAC, (uint8_t *)&cast, sizeof(cast));
  if (sendResult != ESP_OK) {
    Serial.printf("esp_now_send (cast) failed: %s\n", esp_err_to_name(sendResult));
  }

  flashPixel(0, 0, 255); // blue = spell sent, awaiting result
}

// framework-arduinoespressif32 3.x (IDF4-based, what this PlatformIO
// espressif32 platform version resolves) uses the older esp_now_recv_cb_t
// signature (mac, not esp_now_recv_info_t*). If you upgrade the platform to
// a newer arduino-esp32 core, this will need to change to
// (const esp_now_recv_info_t *info, const uint8_t *data, int len).
void onDataRecv(const uint8_t *mac, const uint8_t *data, int len) {
  if (len < 1) return;
  uint8_t msgType = data[0];

  // Every message type below comes from a target, so this is a safe place
  // to track "which target did we most recently hear from" regardless of
  // what kind of message it was - used to pick the right cached UUID (or
  // notice we need a fresh one) when the button gets pressed.
  memcpy(lastTargetMac, mac, 6);
  haveLastTargetMac = true;

  if (msgType == MSG_REGISTER_RESPONSE && len == sizeof(RegisterResponse)) {
    RegisterResponse resp;
    memcpy(&resp, data, sizeof(resp));

    if (resp.success) {
      saveCasterUUID(mac, resp.casterUUID);
      Serial.print("Registered with this target! casterUUID: ");
      Serial.println(resp.casterUUID);
      flashPixel(0, 255, 0, 2);
    } else {
      Serial.println("Registration failed, will retry.");
      flashPixel(255, 0, 0);
    }
  } else if (msgType == MSG_CAST_RESULT && len == sizeof(CastResult)) {
    CastResult result;
    memcpy(&result, data, sizeof(result));

    if (result.success) {
      Serial.println("Spell resolved successfully!");
      flashPixel(0, 255, 0, 2);
    } else {
      Serial.println("Spell fizzled.");
      flashPixel(255, 0, 0, 2);
    }
  } else if (msgType == MSG_TIME_SYNC && len == sizeof(TimeSync)) {
    TimeSync sync;
    memcpy(&sync, data, sizeof(sync));

    bool firstSync = !timeSynced;
    estimatedEpochMs = sync.epochMs;
    millisAtSync = millis();
    lastTimeSyncReceived = millis();
    timeSynced = true;

    // Authoritatively re-pin to whatever channel this packet actually
    // arrived on, as the very last thing this handler does. onDataRecv runs
    // on a different task than loop() (which does the channel hopping) -
    // setting timeSynced=true above doesn't fully close that race, since
    // loop() might read the old (false) value and call hopToNextChannel()
    // in the same narrow window, right before or after this line runs. Re-
    // asserting the correct channel here, after timeSynced is already
    // true, wins that race in practice - if loop() did sneak a hop in
    // first, this overwrites it.
    uint8_t actualChannel;
    wifi_second_chan_t actualSecondChan;
    esp_wifi_get_channel(&actualChannel, &actualSecondChan);
    currentScanChannel = actualChannel;
    esp_wifi_set_channel(currentScanChannel, WIFI_SECOND_CHAN_NONE);

    if (firstSync) {
      Serial.printf("Time synced from target on channel %d - can now register/cast.\n", currentScanChannel);
    }
  }
}

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
  // Temporarily verbose while diagnosing why the target never receives
  // anything from this board - this only reflects local radio TX status,
  // not whether the target actually heard it, but if this never fires at
  // all, or fires with FAIL, that points at the wand's TX path itself
  // rather than anything on the target's end.
  Serial.printf("onDataSent: status=%s\n", status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

// On a WiFi-disconnected STA, esp_wifi_set_channel() doesn't reliably take
// effect for transmit unless promiscuous mode is toggled around the call -
// a well-known ESP-IDF quirk for exactly this "unassociated device wants a
// specific channel" scenario.
void setWiFiChannel(uint8_t channel) {
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
}

void hopToNextChannel() {
  // Race guard: onDataRecv() runs on the WiFi/ESP-NOW task, not loop()'s -
  // timeSynced can flip true between loop()'s "if (!timeSynced)" check and
  // this call actually running. Re-check right before touching the radio so
  // a sync landing in that gap doesn't hop the wand off the channel it was
  // just found on (the TIME_SYNC handler's own re-pin is the other half of
  // closing this race).
  if (timeSynced) return;
  currentScanChannel++;
  if (currentScanChannel > WIFI_MAX_CHANNEL) currentScanChannel = WIFI_MIN_CHANNEL;
  setWiFiChannel(currentScanChannel);
}

void initEspNow() {
  pinMode(RF_SWITCH, OUTPUT);
  digitalWrite(RF_SWITCH, HIGH); // external antenna path
  Serial.println("RF switch set to EXTERNAL antenna.");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  setWiFiChannel(currentScanChannel);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    while (1) { flashPixel(255, 0, 0); delay(500); }
  }

  esp_now_register_recv_cb(onDataRecv);
  esp_now_register_send_cb(onDataSent);

  // 0 = "use whatever esp_wifi_set_channel() actually has the radio on
  // right now" per ESP-NOW's own docs - safer than declaring a specific
  // channel here, since that's a second, independent place a channel value
  // could drift out of sync with the radio's real (actively scanning) state.
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, BROADCAST_MAC, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW broadcast peer!");
  }

  Serial.printf("ESP-NOW ready (broadcast). Scanning for a target starting on channel %d...\n", currentScanChannel);
}

// ============================================================================
// BUTTON
// ============================================================================

void setupButton() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void checkButton() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long now = millis();
    if (now - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = now;

      if (!timeSynced || !haveLastTargetMac) {
        Serial.println("No target detected yet - hang tight.");
        return;
      }

      char *uuid = findCasterUUID(lastTargetMac);
      if (uuid != nullptr) {
        sendCastSpell(uuid);
      } else {
        Serial.println("Not registered with this target yet - sending registration request.");
        sendRegisterRequest();
        lastRegisterAttempt = now;
      }
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

  Serial.println("\n================================");
  Serial.println("  ESP-NOW Wand - TinyS3");
  Serial.println("  Physical MAGIC: wandCast");
  Serial.println("================================\n");

  setupButton();

  // Preferences stays open for the sketch's lifetime rather than
  // begin()/end() per access - the ESP-NOW recv callback (onDataRecv) also
  // writes to it (casterUUID on successful registration), and that callback
  // runs from the WiFi/ESP-NOW task, not from loop().
  preferences.begin(NVS_NAMESPACE, false);
  loadOrGenerateKeys();

  initEspNow();

  // Can't send a validly-signed registration request yet - no time sync
  // received from the target at this point, so any timestamp we'd sign now
  // would fail fount's freshness check. loop() picks this up as soon as
  // timeSynced flips true (and re-registers automatically at any new
  // target it meets from then on, per-target-MAC, without needing this
  // check again).
  Serial.println("Waiting for a target's signal before registering...");

  pixel.setPixelColor(0, pixel.Color(0, 0, 0));
  pixel.show();

  Serial.println("\nReady. Press BOOT to cast.\n");
}

void loop() {
  checkButton();

  if (timeSynced && (millis() - lastTimeSyncReceived > TIME_SYNC_STALE_MS)) {
    Serial.println("Time sync gone stale (moved to a new venue?) - resuming channel scan.");
    timeSynced = false;
  }

  if (!timeSynced) {
    if (millis() - lastChannelHop > CHANNEL_DWELL_MS) {
      hopToNextChannel();
      lastChannelHop = millis();
    }
  }

  // Proactively (re-)register with any target we've heard from but don't
  // have a cached UUID for yet, so a fresh venue is usually already
  // registered by the time someone actually presses the button.
  if (timeSynced && haveLastTargetMac && findCasterUUID(lastTargetMac) == nullptr &&
      (millis() - lastRegisterAttempt > REGISTER_RETRY_MS)) {
    sendRegisterRequest();
    lastRegisterAttempt = millis();
  }

  delay(10);
}
