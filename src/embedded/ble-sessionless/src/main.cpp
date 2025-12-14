/**
 * BLE Sessionless Demo - UM Pro S3
 *
 * Sends "foo" via BLE when touch pad is pressed
 * Works with Swift iOS app that forwards to WebSocket
 *
 * Hardware:
 * - Unexpected Maker Pro S3 (ESP32-S3)
 * - Touch pad on GPIO 4
 * - RGB LED on GPIO 48
 */

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Adafruit_NeoPixel.h>

// BLE UUIDs - matching the Swift app
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// Touch Pin Definition
#define TOUCH_PIN   4
#define TOUCH_THRESHOLD 40000

// RGB LED Pin
#define RGB_LED_PIN 48

// BLE objects
BLEServer* pServer = nullptr;
BLECharacteristic* pCharacteristic = nullptr;
bool deviceConnected = false;

// RGB LED
Adafruit_NeoPixel pixel(1, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// Touch state tracking
bool lastTouchState = false;
uint32_t lastTouchTime = 0;
#define TOUCH_DEBOUNCE_MS 500

// Message counter
uint32_t messageCounter = 0;

/**
 * BLE Server Callbacks
 */
class ServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Device connected!");
        // Set LED to green when connected
        pixel.setPixelColor(0, pixel.Color(0, 255, 0));
        pixel.show();
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Device disconnected!");
        // Set LED to blue when disconnected
        pixel.setPixelColor(0, pixel.Color(0, 0, 255));
        pixel.show();
        // Restart advertising
        BLEDevice::startAdvertising();
        Serial.println("Advertising restarted");
    }
};

/**
 * Initialize BLE
 */
void initBLE() {
    Serial.println("\n=== Initializing BLE ===");

    // Create BLE Device
    BLEDevice::init("MAGIC-ProS3");

    // Create BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create BLE Characteristic
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ |
        BLECharacteristic::PROPERTY_WRITE |
        BLECharacteristic::PROPERTY_NOTIFY |
        BLECharacteristic::PROPERTY_INDICATE
    );

    // Add descriptor for notifications
    pCharacteristic->addDescriptor(new BLE2902());

    // Set initial value
    pCharacteristic->setValue("ready");

    // Start service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("BLE initialized!");
    Serial.println("Device name: MAGIC-ProS3");
    Serial.println("Waiting for client connection...");
}

/**
 * Initialize capacitive touch
 */
void initTouch() {
    Serial.println("\n=== Initializing Touch ===");

    uint16_t touchValue = touchRead(TOUCH_PIN);
    Serial.printf("Touch initialized on GPIO %d\n", TOUCH_PIN);
    Serial.printf("Baseline: %d, Threshold: %d\n", touchValue, TOUCH_THRESHOLD);
    Serial.println("Touch to send 'foo' via BLE!");
}

/**
 * Send message via BLE
 */
void sendMessage(const char* message) {
    if (!deviceConnected) {
        Serial.println("No device connected - cannot send");
        return;
    }

    // Create JSON payload
    char payload[128];
    snprintf(payload, sizeof(payload),
        "{\"msg\":\"%s\",\"counter\":%u,\"timestamp\":%lu}",
        message, messageCounter++, millis());

    // Send via BLE
    pCharacteristic->setValue(payload);
    pCharacteristic->notify();

    Serial.println("\n=== Message Sent ===");
    Serial.printf("Payload: %s\n", payload);
    Serial.println("====================\n");

    // Flash LED white briefly
    pixel.setPixelColor(0, pixel.Color(255, 255, 255));
    pixel.show();
    delay(100);
    pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Back to green
    pixel.show();
}

/**
 * Handle touch input
 */
void handleTouch() {
    uint16_t touchValue = touchRead(TOUCH_PIN);
    bool currentTouchState = (touchValue < TOUCH_THRESHOLD);

    // Detect rising edge with debouncing
    if (currentTouchState && !lastTouchState) {
        uint32_t currentTime = millis();

        if (currentTime - lastTouchTime > TOUCH_DEBOUNCE_MS) {
            Serial.println("\n*** TOUCH DETECTED! ***");
            Serial.printf("Touch value: %d\n", touchValue);

            sendMessage("foo");

            lastTouchTime = currentTime;
        }
    }

    lastTouchState = currentTouchState;
}

void setup() {
    Serial.begin(115200);
    while (!Serial);

    Serial.println("\n\n================================");
    Serial.println("BLE Sessionless Demo");
    Serial.println("Touch → BLE → iOS → WebSocket");
    Serial.println("================================\n");

    // Initialize RGB LED - start blue (disconnected)
    pixel.begin();
    pixel.setPixelColor(0, pixel.Color(0, 0, 255)); // Blue
    pixel.show();

    // Initialize components
    initBLE();
    initTouch();

    Serial.println("\n=== Ready! ===");
    Serial.println("LED Status:");
    Serial.println("  Blue   = Waiting for connection");
    Serial.println("  Green  = Connected");
    Serial.println("  White  = Sending message");
}

void loop() {
    handleTouch();
    delay(10);
}
