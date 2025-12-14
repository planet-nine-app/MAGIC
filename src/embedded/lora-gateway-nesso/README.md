# LoRa Gateway - Nesso N1

LoRa-to-WebSocket gateway using the Arduino Nesso N1. Receives signed LoRa messages and forwards them to a WebSocket server.

## Overview

This gateway bridges LoRa radio communications to the internet via WiFi and WebSocket. It:
- Receives multi-packet LoRa messages
- Reassembles fragmented transmissions
- Verifies Sessionless cryptographic signatures
- Forwards verified messages to a WebSocket server
- Provides signal quality metrics (RSSI, SNR)

## Hardware Requirements

### Main Board
- **Arduino Nesso N1** (ESP32-C6 based)
  - WiFi 6 (802.11ax)
  - Bluetooth 5.0
  - USB-C programming
  - 4MB Flash

### LoRa Module
- **SX1276** or **SX1278** based LoRa module
- Recommended: RFM95W, Hope RF96, or similar
- Frequency: 915 MHz (North America) or 868 MHz (Europe)

### Wiring

**LoRa Module to Nesso N1:**

| LoRa Pin | Nesso N1 GPIO | Description |
|----------|---------------|-------------|
| NSS      | GPIO 10       | SPI Chip Select |
| RST      | GPIO 9        | Reset |
| DIO0     | GPIO 8        | Interrupt (Packet RX/TX) |
| MOSI     | GPIO 6        | SPI MOSI |
| MISO     | GPIO 5        | SPI MISO |
| SCK      | GPIO 4        | SPI Clock |
| VCC      | 3.3V          | Power |
| GND      | GND           | Ground |

**Important**: The Nesso N1 operates at 3.3V. Do NOT connect 5V to any GPIO pins.

## Features

### ✅ Multi-Packet Reassembly
- Receives and reassembles fragmented messages (up to 4 packets)
- 5-second timeout for incomplete messages
- Handles out-of-order packets

### ✅ Cryptographic Verification
- Verifies Sessionless (secp256k1) signatures
- Validates message authenticity before forwarding
- Reports verification status in WebSocket payload

### ✅ WiFi Connectivity
- Automatic connection to configured WiFi network
- Auto-reconnection on WiFi drop
- Connection status monitoring

### ✅ WebSocket Client
- Forwards verified messages to WebSocket server
- Automatic reconnection on disconnect
- JSON payload format

### ✅ Signal Quality Metrics
- RSSI (Received Signal Strength Indicator)
- SNR (Signal-to-Noise Ratio)
- Per-message quality reporting

## Configuration

Edit the following constants in `src/main.cpp`:

### WiFi Configuration
```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
```

### WebSocket Configuration
```cpp
const char* WS_HOST = "your-server.com";  // or IP like "192.168.1.100"
const uint16_t WS_PORT = 8080;
const char* WS_PATH = "/lora";  // WebSocket endpoint path
```

### LoRa Settings
```cpp
#define LORA_FREQUENCY  915E6  // 915 MHz or 868 MHz
#define LORA_SPREADING  7      // 7-12 (must match transmitter)
#define LORA_BANDWIDTH  125E3  // Must match transmitter
```

## Quick Start

### 1. Install PlatformIO
```bash
# VS Code: Install PlatformIO IDE extension
# Or via CLI:
pip install platformio
```

### 2. Configure WiFi and WebSocket
Edit `src/main.cpp` with your WiFi credentials and WebSocket server details.

### 3. Build and Upload
```bash
cd MAGIC/src/embedded/lora-gateway-nesso
pio run --target upload
```

### 4. Monitor Serial Output
```bash
pio device monitor
```

You should see:
```
================================
LoRa Gateway - Nesso N1
LoRa → WiFi → WebSocket
================================

=== Connecting to WiFi ===
SSID: YourNetwork
..........
[WiFi] Connected!
IP Address: 192.168.1.100

=== Initializing WebSocket ===
Host: your-server.com:8080
Path: /lora
[WS] Connected to your-server.com:8080/lora

=== Initializing LoRa ===
[LoRa] Initialized successfully!

=== Gateway Ready ===
Listening for LoRa messages...
```

## WebSocket Message Format

When a complete LoRa message is received and verified, the gateway sends a JSON message:

```json
{
  "type": "lora_message",
  "message": "Hello, World!",
  "senderPubKey": "02a1b2c3d4e5f6...",
  "signatureValid": true,
  "rssi": -45,
  "snr": 9.5,
  "counter": 42,
  "timestamp": 123456,
  "gatewayTime": 987654
}
```

### Fields
- `type`: Always "lora_message" for LoRa messages
- `message`: The decrypted message content
- `senderPubKey`: Public key of the sender (hex string)
- `signatureValid`: Boolean indicating signature verification result
- `rssi`: Received Signal Strength Indicator (dBm)
- `snr`: Signal-to-Noise Ratio (dB)
- `counter`: Message counter from sender
- `timestamp`: Timestamp from sender (milliseconds)
- `gatewayTime`: Gateway's local time when message was received (milliseconds)

## WebSocket Server Example

Here's a simple Node.js WebSocket server to receive messages:

```javascript
const WebSocket = require('ws');

const wss = new WebSocket.Server({ port: 8080 });

wss.on('connection', (ws) => {
  console.log('Gateway connected');

  ws.on('message', (data) => {
    const message = JSON.parse(data);

    if (message.type === 'lora_message') {
      console.log('LoRa Message Received:');
      console.log(`  From: ${message.senderPubKey.substring(0, 16)}...`);
      console.log(`  Message: ${message.message}`);
      console.log(`  Valid: ${message.signatureValid}`);
      console.log(`  RSSI: ${message.rssi} dBm`);
      console.log(`  SNR: ${message.snr} dB`);
    }
  });

  ws.on('close', () => {
    console.log('Gateway disconnected');
  });
});

console.log('WebSocket server listening on port 8080');
```

## Troubleshooting

### WiFi Connection Issues
- Verify SSID and password are correct
- Check WiFi signal strength at gateway location
- Ensure WiFi network is 2.4GHz (ESP32-C6 supports both 2.4GHz and 5GHz)
- Check serial monitor for connection status

### WebSocket Connection Issues
- Verify WebSocket server is running and accessible
- Check firewall settings on server
- Test WebSocket endpoint with a different client
- Ensure WS_HOST, WS_PORT, and WS_PATH are correct

### LoRa Reception Issues
- Verify LoRa frequency matches transmitter (915/868 MHz)
- Ensure spreading factor and bandwidth match transmitter
- Check antenna connections on both sides
- Verify wiring between LoRa module and Nesso N1
- Check RSSI values (should be > -120 dBm for good reception)

### Signature Verification Fails
- Ensure both transmitter and gateway use the same Sessionless library version
- Check that the complete message was reassembled (all chunks received)
- Verify no packet corruption (check SNR values)

## Performance

### Reception Capacity
- Can receive **~720 bytes** per message (4 packets × 180 bytes)
- Reassembly time: ~40ms
- Signature verification: ~20ms

### Network Latency
- WiFi connection: <10ms
- WebSocket send: <50ms
- **Total gateway latency**: <100ms

### Power Consumption
- Active (receiving): ~100 mA
- WiFi connected: ~80 mA
- Idle: ~50 mA
- Deep sleep: <10 µA (future feature)

## Architecture

```
┌──────────────┐
│ LoRa Module  │
│  (SX1276)    │
└──────┬───────┘
       │ SPI
┌──────▼───────┐
│  Nesso N1    │
│  (ESP32-C6)  │
│              │
│  - Receive   │
│  - Reassemble│
│  - Verify    │
└──────┬───────┘
       │ WiFi
┌──────▼───────┐
│  WebSocket   │
│   Server     │
└──────────────┘
```

## Compatibility

This gateway is designed to work with:
- **Transmitter**: LoRa Sessionless project (`lora-sessionless/`)
- **Hardware**: Any LoRa device using the same frequency and settings
- **Protocol**: Multi-packet fragmentation with single signature

## Future Enhancements

- [ ] Deep sleep mode between messages for battery operation
- [ ] Local web interface for configuration
- [ ] MQTT support in addition to WebSocket
- [ ] Over-the-air (OTA) updates
- [ ] Message queueing during network outages
- [ ] Multi-gateway mesh networking
- [ ] Encryption layer for WebSocket transmission

## Related Projects

- **LoRa Transmitter**: `/MAGIC/src/embedded/lora-sessionless/`
- **MAGIC Protocol**: `/MAGIC/docs/`
- **Sessionless**: `/sessionless/`

## License

Part of the Planet Nine MAGIC protocol ecosystem.

## Author

Planet Nine Development Team
December 2025
