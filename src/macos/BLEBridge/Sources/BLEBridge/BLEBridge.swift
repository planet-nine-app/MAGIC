import Foundation
@preconcurrency import CoreBluetooth

class BLEBridge: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    // BLE UUIDs - matching the ESP32
    let SERVICE_UUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let CHARACTERISTIC_UUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")

    // HTTP configuration
    let SERVER_URL = "http://localhost:8081/lora"

    var centralManager: CBCentralManager!
    var discoveredPeripheral: CBPeripheral?
    var targetCharacteristic: CBCharacteristic?

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
        print("✅ HTTP bridge ready - will POST to \(SERVER_URL)")
    }

    // MARK: - BLE Central Manager Delegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("✅ Bluetooth is powered on")
            print("🔍 Scanning for MAGIC-ProS3...")
            centralManager.scanForPeripherals(withServices: [SERVICE_UUID], options: nil)

        case .poweredOff:
            print("❌ Bluetooth is powered off")

        case .resetting:
            print("⚠️  Bluetooth is resetting")

        case .unauthorized:
            print("❌ Bluetooth is unauthorized")

        case .unsupported:
            print("❌ Bluetooth is not supported")

        case .unknown:
            print("❓ Bluetooth state is unknown")

        @unknown default:
            print("❓ Unknown Bluetooth state")
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print("📡 Discovered: \(peripheral.name ?? "Unknown") (RSSI: \(RSSI))")

        // Check if this is our device
        if peripheral.name == "MAGIC-ProS3" {
            print("🎯 Found MAGIC-ProS3!")
            discoveredPeripheral = peripheral
            centralManager.stopScan()
            centralManager.connect(peripheral, options: nil)
        }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("✅ Connected to \(peripheral.name ?? "device")")
        peripheral.delegate = self
        peripheral.discoverServices([SERVICE_UUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: (any Error)?) {
        print("❌ Failed to connect: \(String(describing: error))")
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: (any Error)?) {
        print("❌ Disconnected from \(peripheral.name ?? "device")")
        if let error = error {
            print("   Error: \(error)")
        }

        // Reconnect
        print("🔄 Attempting to reconnect...")
        centralManager.connect(peripheral, options: nil)
    }

    // MARK: - BLE Peripheral Delegate

    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: (any Error)?) {
        if let error = error {
            print("❌ Error discovering services: \(error)")
            return
        }

        guard let services = peripheral.services else { return }

        for service in services {
            print("🔧 Discovered service: \(service.uuid)")
            peripheral.discoverCharacteristics([CHARACTERISTIC_UUID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: (any Error)?) {
        if let error = error {
            print("❌ Error discovering characteristics: \(error)")
            return
        }

        guard let characteristics = service.characteristics else { return }

        for characteristic in characteristics {
            print("📝 Discovered characteristic: \(characteristic.uuid)")

            if characteristic.uuid == CHARACTERISTIC_UUID {
                targetCharacteristic = characteristic
                // Subscribe to notifications
                peripheral.setNotifyValue(true, for: characteristic)
                print("🔔 Subscribed to notifications")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        if let error = error {
            print("❌ Error reading characteristic: \(error)")
            return
        }

        guard let data = characteristic.value else { return }
        guard let message = String(data: data, encoding: .utf8) else {
            print("⚠️  Could not decode message")
            return
        }

        print("\n🎉 === BLE MESSAGE RECEIVED ===")
        print("📨 Message: \(message)")
        print("==============================\n")

        // Forward via HTTP POST
        sendToServer(message: message)
    }

    // MARK: - HTTP Posting

    func sendToServer(message: String) {
        guard let url = URL(string: SERVER_URL) else {
            print("⚠️  Invalid server URL")
            return
        }

        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")

        let payload: [String: Any] = [
            "type": "lora_message",
            "message": message,
            "senderPubKey": "BLE-Device",
            "signatureValid": true,
            "rssi": -50,
            "snr": 10.0,
            "counter": 0,
            "timestamp": Int(Date().timeIntervalSince1970 * 1000),
            "gatewayTime": Int(Date().timeIntervalSince1970 * 1000)
        ]

        do {
            request.httpBody = try JSONSerialization.data(withJSONObject: payload)

            let task = URLSession.shared.dataTask(with: request) { data, response, error in
                if let error = error {
                    print("❌ HTTP Error: \(error.localizedDescription)")
                    return
                }

                if let httpResponse = response as? HTTPURLResponse {
                    if httpResponse.statusCode == 200 {
                        print("📤 Successfully forwarded to server!")
                    } else {
                        print("⚠️  Server returned status \(httpResponse.statusCode)")
                    }
                }
            }
            task.resume()
        } catch {
            print("❌ Failed to serialize JSON: \(error)")
        }
    }
}

// MARK: - Main

print("""
================================================
🔮 MAGIC BLE Bridge
================================================
BLE Device: MAGIC-ProS3
Server: http://localhost:8081/lora
================================================

Starting bridge...

""")

let bridge = BLEBridge()
RunLoop.main.run()
