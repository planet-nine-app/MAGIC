/**
 * LoRa Gateway - Nesso N1
 *
 * Receives signed LoRa messages and forwards them via WebSocket
 *
 * Hardware:
 * - Arduino Nesso N1 (ESP32-C6)
 * - LoRa module (SX1276/SX1278)
 *
 * Pin Configuration (Nesso N1):
 * - LoRa NSS:   GPIO 10
 * - LoRa RST:   GPIO 9
 * - LoRa DIO0:  GPIO 8
 * - LoRa MOSI:  GPIO 6
 * - LoRa MISO:  GPIO 5
 * - LoRa SCK:   GPIO 4
 */

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include "../lib/sessionless.h"

// WiFi Configuration
const char* WIFI_SSID = "Wildwood";
const char* WIFI_PASSWORD = "arloiscute!";

// WebSocket Configuration
const char* WS_HOST = "192.168.0.77";  // Your computer's IP address
const uint16_t WS_PORT = 8081;
const char* WS_PATH = "/lora";  // WebSocket endpoint path

// LoRa Pin Definitions for Nesso N1
#define LORA_NSS    10
#define LORA_RST    9
#define LORA_DIO0   8
#define LORA_MOSI   6
#define LORA_MISO   5
#define LORA_SCK    4

// LoRa Settings (must match transmitter)
#define LORA_FREQUENCY  915E6  // 915 MHz (North America)
#define LORA_TX_POWER   20     // Max TX power
#define LORA_SPREADING  7      // Spreading factor (7-12)
#define LORA_BANDWIDTH  125E3  // 125 kHz bandwidth

// Multi-packet reassembly settings (must match transmitter)
#define MESSAGE_TIMEOUT_MS 5000  // 5 second timeout for reassembly

// Message reassembly buffer
struct MessageBuffer {
    String msgId;
    uint8_t totalChunks;
    uint8_t receivedChunks;
    String chunks[4];  // Support up to 4 chunks
    uint32_t lastReceived;
    bool complete;
} messageBuffer;

// WebSocket client
WebSocketsClient webSocket;

// Status tracking
bool wifiConnected = false;
bool wsConnected = false;
uint32_t lastReconnectAttempt = 0;
#define RECONNECT_INTERVAL 5000  // Try reconnecting every 5 seconds

/**
 * WebSocket event handler
 */
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("[WS] Disconnected");
            wsConnected = false;
            break;

        case WStype_CONNECTED:
            Serial.printf("[WS] Connected to %s:%d%s\n", WS_HOST, WS_PORT, WS_PATH);
            wsConnected = true;

            // Send connection message
            webSocket.sendTXT("{\"type\":\"gateway_connected\",\"device\":\"nesso_n1\"}");
            break;

        case WStype_TEXT:
            Serial.printf("[WS] Received: %s\n", payload);
            break;

        case WStype_ERROR:
            Serial.println("[WS] Error");
            wsConnected = false;
            break;

        default:
            break;
    }
}

/**
 * Initialize WiFi connection
 */
void initWiFi() {
    Serial.println("\n=== Connecting to WiFi ===");
    Serial.printf("SSID: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n[WiFi] Connected!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("Signal Strength: %d dBm\n", WiFi.RSSI());
    } else {
        wifiConnected = false;
        Serial.println("\n[WiFi] Connection failed!");
    }
}

/**
 * Initialize WebSocket connection
 */
void initWebSocket() {
    Serial.println("\n=== Initializing WebSocket ===");
    Serial.printf("Host: %s:%d\n", WS_HOST, WS_PORT);
    Serial.printf("Path: %s\n", WS_PATH);

    webSocket.begin(WS_HOST, WS_PORT, WS_PATH);
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
}

/**
 * Initialize LoRa module
 */
void initLoRa() {
    Serial.println("\n=== Initializing LoRa ===");

    // Set custom SPI pins for Nesso N1
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    // Set LoRa pins
    LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

    // Initialize LoRa
    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("[LoRa] Init failed!");
        while (1) {
            delay(1000);
            Serial.println("[LoRa] Retrying...");
            if (LoRa.begin(LORA_FREQUENCY)) break;
        }
    }

    // Configure LoRa settings
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSpreadingFactor(LORA_SPREADING);
    LoRa.setSignalBandwidth(LORA_BANDWIDTH);

    Serial.println("[LoRa] Initialized successfully!");
    Serial.printf("Frequency: %.2f MHz\n", LORA_FREQUENCY / 1e6);
    Serial.printf("Spreading Factor: %d\n", LORA_SPREADING);
}

/**
 * Send message via WebSocket
 */
void sendToWebSocket(const String& message, const char* senderPubKey, bool signatureValid,
                     int rssi, float snr, uint32_t counter, uint32_t timestamp) {
    if (!wsConnected) {
        Serial.println("[WS] Not connected, cannot send");
        return;
    }

    // Create JSON document
    StaticJsonDocument<1024> doc;
    doc["type"] = "lora_message";
    doc["message"] = message;
    doc["senderPubKey"] = senderPubKey;
    doc["signatureValid"] = signatureValid;
    doc["rssi"] = rssi;
    doc["snr"] = snr;
    doc["counter"] = counter;
    doc["timestamp"] = timestamp;
    doc["gatewayTime"] = millis();

    // Serialize and send
    String output;
    serializeJson(doc, output);

    webSocket.sendTXT(output);
    Serial.println("[WS] Forwarded message to server");
}

/**
 * Receive and verify a signed message with multi-packet reassembly
 */
void receiveSignedMessage() {
    int packetSize = LoRa.parsePacket();
    if (packetSize == 0) return;

    // Capture RSSI and SNR
    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    // Read packet
    String received = "";
    while (LoRa.available()) {
        received += (char)LoRa.read();
    }

    Serial.println("\n=== Received LoRa Packet ===");
    Serial.printf("RSSI: %d dBm\n", rssi);
    Serial.printf("SNR: %.2f dB\n", snr);
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

        // Forward to WebSocket
        if (message && pubKeyHex) {
            sendToWebSocket(String(message), pubKeyHex, valid, rssi, snr, counter, timestamp);
        }

        // Clear buffer for next message
        messageBuffer.msgId = "";
        messageBuffer.receivedChunks = 0;
        messageBuffer.complete = false;
    }
}

/**
 * Maintain WiFi connection
 */
void maintainWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.println("[WiFi] Connection lost!");
            wifiConnected = false;
            wsConnected = false;
        }

        // Try to reconnect
        if (millis() - lastReconnectAttempt > RECONNECT_INTERVAL) {
            Serial.println("[WiFi] Attempting reconnection...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            lastReconnectAttempt = millis();
        }
    } else if (!wifiConnected) {
        wifiConnected = true;
        Serial.println("[WiFi] Reconnected!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());

        // Reinitialize WebSocket
        initWebSocket();
    }
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n\n================================");
    Serial.println("LoRa Gateway - Nesso N1");
    Serial.println("LoRa → WiFi → WebSocket");
    Serial.println("================================\n");

    // Initialize components
    initWiFi();

    if (wifiConnected) {
        initWebSocket();
    }

    initLoRa();

    Serial.println("\n=== Gateway Ready ===");
    Serial.println("Listening for LoRa messages...\n");
}

void loop() {
    // Maintain connections
    maintainWiFi();

    if (wifiConnected) {
        webSocket.loop();
    }

    // Always listen for incoming LoRa messages
    receiveSignedMessage();

    delay(10);
}
