/**
 * LoRa Sessionless - Signed Radio Messages
 *
 * Uses UM Pro S3 + LoRa module for long-range signed message transmission
 * Messages are signed using Sessionless cryptographic signatures
 *
 * Hardware:
 * - Unexpected Maker Pro S3 (ESP32-S3)
 * - LoRa module (SX1276/SX1278)
 *
 * Pin Configuration (UM Pro S3):
 * - LoRa NSS:   GPIO 5
 * - LoRa RST:   GPIO 14
 * - LoRa DIO0:  GPIO 13
 * - LoRa MOSI:  GPIO 35
 * - LoRa MISO:  GPIO 37
 * - LoRa SCK:   GPIO 36
 * - Touch:      GPIO 4 (Touch pad on UM Pro S3)
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <sessionless.h>

// LoRa Pin Definitions for UM Pro S3
#define LORA_NSS    5
#define LORA_RST    14
#define LORA_DIO0   13
#define LORA_MOSI   35
#define LORA_MISO   37
#define LORA_SCK    36

// Touch Pin Definition
#define TOUCH_PIN   4         // GPIO 4 (T4 - Touch Channel 4)
#define TOUCH_THRESHOLD 40000  // Threshold for touch detection (adjust as needed)

// LoRa Settings
#define LORA_FREQUENCY  915E6  // 915 MHz (North America)
#define LORA_TX_POWER   20     // Max TX power
#define LORA_SPREADING  7      // Spreading factor (7-12)
#define LORA_BANDWIDTH  125E3  // 125 kHz bandwidth

// Sessionless keys (stored in NVS in production)
uint8_t privateKey[32];
uint8_t publicKey[64];

// Message counter for uniqueness
uint32_t messageCounter = 0;

// Touch state tracking
bool lastTouchState = false;
uint32_t lastTouchTime = 0;
#define TOUCH_DEBOUNCE_MS 500  // Debounce time in milliseconds

// Multi-packet transmission settings
#define MAX_PACKET_SIZE 180        // Max data per packet (bytes)
#define PACKET_DELAY_MS 100        // Delay between packets (ms)

// Message reassembly buffer
struct MessageBuffer {
    String msgId;
    uint8_t totalChunks;
    uint8_t receivedChunks;
    String chunks[4];  // Support up to 4 chunks
    uint32_t lastReceived;
    bool complete;
} messageBuffer;

#define MESSAGE_TIMEOUT_MS 5000  // 5 second timeout for reassembly

// Forward declarations
void sendSignedMessage(const char* message);

/**
 * Initialize sessionless cryptography
 */
void initSessionless() {
    Serial.println("Initializing Sessionless...");

    // TODO: Load keys from NVS (Non-Volatile Storage)
    // For now, generate new keys (will be different each boot)
    generateKeys(privateKey, publicKey);

    Serial.print("Public Key: ");
    for(int i = 0; i < 64; i++) {
        Serial.printf("%02x", publicKey[i]);
    }
    Serial.println();
}

/**
 * Initialize capacitive touch
 */
void initTouch() {
    Serial.println("Initializing Touch Sensor...");

    // Touch pins are automatically configured when using touchRead()
    // Read initial value to calibrate
    uint16_t touchValue = touchRead(TOUCH_PIN);

    Serial.printf("Touch initialized on GPIO %d\n", TOUCH_PIN);
    Serial.printf("Baseline touch value: %d\n", touchValue);
    Serial.printf("Touch threshold: %d\n", TOUCH_THRESHOLD);
    Serial.println("Touch the pad to send 'Hello, World!' over LoRa");
}

/**
 * Check for touch input and send message
 */
void handleTouch() {
    uint16_t touchValue = touchRead(TOUCH_PIN);
    bool currentTouchState = (touchValue < TOUCH_THRESHOLD);

    // Detect rising edge (touch started) with debouncing
    if (currentTouchState && !lastTouchState) {
        uint32_t currentTime = millis();

        // Check if enough time has passed since last touch
        if (currentTime - lastTouchTime > TOUCH_DEBOUNCE_MS) {
            Serial.println("\n*** TOUCH DETECTED! ***");
            Serial.printf("Touch value: %d (threshold: %d)\n", touchValue, TOUCH_THRESHOLD);

            // Send "Hello, World!" message
            sendSignedMessage("Hello, World!");

            lastTouchTime = currentTime;
        }
    }

    lastTouchState = currentTouchState;
}

/**
 * Initialize LoRa module
 */
void initLoRa() {
    Serial.println("Initializing LoRa...");

    // Set custom SPI pins for UM Pro S3
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // Set LoRa pins
    LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

    // Initialize LoRa
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("LoRa init failed!");
        while (1);
    }

    // Configure LoRa settings
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSpreadingFactor(LORA_SPREADING);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);

    Serial.println("LoRa initialized successfully!");
    Serial.printf("Frequency: %.2f MHz\n", LORA_FREQUENCY / 1e6);
    Serial.printf("TX Power: %d dBm\n", LORA_TX_POWER);
    Serial.printf("Spreading Factor: %d\n", LORA_SPREADING);
}

/**
 * Sign and send a message over LoRa using multi-packet fragmentation
 */
void sendSignedMessage(const char* message) {
    // Create JSON document with message data
    StaticJsonDocument<512> doc;
    doc["msg"] = message;
    doc["counter"] = messageCounter++;
    doc["timestamp"] = millis();

    // Serialize to string for signing
    String payload;
    serializeJson(doc, payload);

    // Generate unique message ID
    char msgId[32];
    sprintf(msgId, "%u-%u", messageCounter - 1, (uint32_t)millis());

    // Calculate number of chunks needed
    int totalLength = payload.length();
    int numChunks = (totalLength + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
    if (numChunks > 4) numChunks = 4; // Cap at 4 chunks

    Serial.printf("\n=== Sending Multi-Packet Message ===\n");
    Serial.printf("Message ID: %s\n", msgId);
    Serial.printf("Total size: %d bytes\n", totalLength);
    Serial.printf("Chunks: %d\n", numChunks);

    // Sign the complete payload (before fragmentation)
    uint8_t signature[64];
    sign((const uint8_t*)payload.c_str(), payload.length(), privateKey, signature);

    // Convert signature to hex
    char sigHex[129];
    for(int i = 0; i < 64; i++) {
        sprintf(&sigHex[i*2], "%02x", signature[i]);
    }

    // Convert public key to hex
    char pubKeyHex[129];
    for(int i = 0; i < 64; i++) {
        sprintf(&pubKeyHex[i*2], "%02x", publicKey[i]);
    }

    // Send chunks
    for (int i = 0; i < numChunks; i++) {
        StaticJsonDocument<256> chunkDoc;
        chunkDoc["msgId"] = msgId;
        chunkDoc["seq"] = i + 1;
        chunkDoc["total"] = numChunks;

        // Extract chunk data
        int start = i * MAX_PACKET_SIZE;
        int end = min(start + MAX_PACKET_SIZE, totalLength);
        String chunkData = payload.substring(start, end);
        chunkDoc["data"] = chunkData;

        // Add signature and public key only on final packet
        if (i == numChunks - 1) {
            chunkDoc["sig"] = sigHex;
            chunkDoc["pubKey"] = pubKeyHex;
        }

        // Serialize chunk
        String chunkPayload;
        serializeJson(chunkDoc, chunkPayload);

        // Send chunk over LoRa
        LoRa.beginPacket();
        LoRa.print(chunkPayload);
        LoRa.endPacket();

        Serial.printf("Sent chunk %d/%d (%d bytes)\n", i + 1, numChunks, chunkPayload.length());

        // Delay between packets (except after last one)
        if (i < numChunks - 1) {
            delay(PACKET_DELAY_MS);
        }
    }

    Serial.println("=== Multi-Packet Send Complete ===\n");
}

/**
 * Receive and verify a signed message with multi-packet reassembly
 */
void receiveSignedMessage() {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    // Read packet
    String received = "";
    while (LoRa.available()) {
        received += (char)LoRa.read();
    }

    Serial.println("\n=== Received LoRa Packet ===");
    Serial.printf("RSSI: %d dBm\n", LoRa.packetRssi());
    Serial.printf("SNR: %.2f dB\n", LoRa.packetSnr());
    Serial.printf("Size: %d bytes\n", packetSize);

    // Parse JSON chunk
    StaticJsonDocument<256> chunkDoc;
    DeserializationError error = deserializeJson(chunkDoc, received);

    if (error) {
        Serial.println("Failed to parse JSON!");
        return;
    }

    // Extract chunk metadata
    const char* msgId = chunkDoc["msgId"];
    uint8_t seq = chunkDoc["seq"];
    uint8_t total = chunkDoc["total"];
    const char* data = chunkDoc["data"];

    if (!msgId || seq == 0 || total == 0 || !data) {
        Serial.println("Missing chunk metadata!");
        return;
    }

    Serial.printf("Chunk %d/%d for message %s\n", seq, total, msgId);

    // Check for timeout on existing buffer
    if (messageBuffer.msgId.length() > 0 &&
        messageBuffer.msgId != msgId &&
        (millis() - messageBuffer.lastReceived) > MESSAGE_TIMEOUT_MS) {
        Serial.println("Previous message timed out, clearing buffer");
        messageBuffer.msgId = "";
        messageBuffer.receivedChunks = 0;
        messageBuffer.complete = false;
    }

    // Initialize new message buffer if needed
    if (messageBuffer.msgId.length() == 0 || messageBuffer.msgId != msgId) {
        messageBuffer.msgId = String(msgId);
        messageBuffer.totalChunks = total;
        messageBuffer.receivedChunks = 0;
        messageBuffer.complete = false;
        for (int i = 0; i < 4; i++) {
            messageBuffer.chunks[i] = "";
        }
        Serial.printf("Started reassembly for message %s (%d chunks expected)\n", msgId, total);
    }

    // Store chunk data
    if (seq > 0 && seq <= 4) {
        messageBuffer.chunks[seq - 1] = String(data);
        messageBuffer.receivedChunks++;
        messageBuffer.lastReceived = millis();
        Serial.printf("Stored chunk %d (%d/%d received)\n", seq, messageBuffer.receivedChunks, total);
    }

    // Check if all chunks received
    if (messageBuffer.receivedChunks == messageBuffer.totalChunks) {
        Serial.println("\n=== All Chunks Received - Reassembling ===");

        // Reassemble complete payload
        String completePayload = "";
        for (int i = 0; i < messageBuffer.totalChunks; i++) {
            completePayload += messageBuffer.chunks[i];
        }

        Serial.printf("Reassembled payload: %d bytes\n", completePayload.length());

        // Extract signature and public key from final chunk
        const char* sigHex = chunkDoc["sig"];
        const char* pubKeyHex = chunkDoc["pubKey"];

        if (!sigHex || !pubKeyHex) {
            Serial.println("Missing signature or public key in final chunk!");
            messageBuffer.msgId = "";
            return;
        }

        // Convert hex strings to bytes
        uint8_t signature[64];
        uint8_t senderPubKey[64];

        for(int i = 0; i < 64; i++) {
            sscanf(&sigHex[i*2], "%2hhx", &signature[i]);
            sscanf(&pubKeyHex[i*2], "%2hhx", &senderPubKey[i]);
        }

        // Verify signature against complete payload
        bool valid = verify(
            (const uint8_t*)completePayload.c_str(),
            completePayload.length(),
            signature,
            senderPubKey
        );

        // Parse complete payload
        StaticJsonDocument<512> payloadDoc;
        deserializeJson(payloadDoc, completePayload);

        const char* message = payloadDoc["msg"];
        uint32_t counter = payloadDoc["counter"];
        uint32_t timestamp = payloadDoc["timestamp"];

        // Display results
        Serial.println("\n=== Verification Result ===");
        Serial.printf("Signature: %s\n", valid ? "VALID ✓" : "INVALID ✗");
        Serial.printf("Message: %s\n", message ? message : "N/A");
        Serial.printf("Counter: %u\n", counter);
        Serial.printf("Timestamp: %u ms\n", timestamp);
        Serial.println("===========================\n");

        // Clear buffer for next message
        messageBuffer.msgId = "";
        messageBuffer.receivedChunks = 0;
        messageBuffer.complete = false;
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n\n=================================");
    Serial.println("LoRa Sessionless Demo");
    Serial.println("Signed Radio Messages + Touch");
    Serial.println("=================================\n");

    // Initialize components
    initSessionless();
    initLoRa();
    initTouch();

    Serial.println("\nReady!");
    Serial.println("Commands:");
    Serial.println("  send <message> - Send a signed message");
    Serial.println("  touch GPIO 4   - Send 'Hello, World!'");
    Serial.println("  listening...   - Receiving mode active");
}

void loop() {
    // Check for touch input
    handleTouch();

    // Check for serial commands
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        if (command.startsWith("send ")) {
            String message = command.substring(5);
            sendSignedMessage(message.c_str());
        } else {
            Serial.println("Unknown command. Use: send <message>");
        }
    }

    // Always listen for incoming messages
    receiveSignedMessage();

    delay(10);
}
