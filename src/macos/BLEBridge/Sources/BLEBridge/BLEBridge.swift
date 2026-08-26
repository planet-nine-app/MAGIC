import Foundation
@preconcurrency import CoreBluetooth

/// Generic BLE central for ../../embedded/ble-sessionless/ - a single ESP32
/// board that owns its own Sessionless identity and casts a real, signed
/// MAGIC spell (wandCast) against fount whenever its BLE characteristic is
/// written to. This app deliberately does none of that signing itself: it
/// only discovers the board, writes a trigger byte on request, and prints/
/// relays whatever real cast-result JSON the board notifies back. That's
/// the whole point of this transport - a laptop (or phone, or Apple TV)
/// doesn't need any MAGIC-aware code of its own to trigger a spell near a
/// BLE peripheral that does.
///
/// Previously this app faked a LoRa-gateway payload (hardcoded
/// signatureValid: true) just to forward the old plaintext BLE stub into
/// lora-server's /lora route and borrow its display. Now that the ESP32
/// side casts a real spell and notifies a real result, that fakery is gone
/// - see git history if you need the old version.
class BLEBridge: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    // BLE UUIDs - must match ../../embedded/ble-sessionless/src/main.cpp exactly.
    let SERVICE_UUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    let CHARACTERISTIC_UUID = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    let DEVICE_NAME = "MAGIC-ProS3"

    // Optional: same big-screen demo display the ESP-NOW target posts to
    // (../../embedded/espnow-demo-server/) - best-effort only, exactly like
    // the firmware's own notifyDemoServer(). Never blocks or fails anything
    // if it's not running.
    let DEMO_SERVER_URL = "http://localhost:4747/cast"

    var centralManager: CBCentralManager!
    var discoveredPeripheral: CBPeripheral?
    var targetCharacteristic: CBCharacteristic?

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    // MARK: - BLE Central Manager Delegate

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            print("✅ Bluetooth is powered on")
            print("🔍 Scanning for \(DEVICE_NAME)...")
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

        if peripheral.name == DEVICE_NAME {
            print("🎯 Found \(DEVICE_NAME)!")
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
        targetCharacteristic = nil

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
                peripheral.setNotifyValue(true, for: characteristic)
                print("🔔 Subscribed to notifications")
                print("💡 Type \"cast\" + Enter to trigger a spell cast on the board.")
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
            print("⚠️  Could not decode notified value")
            return
        }

        print("\n🎉 === BLE NOTIFY RECEIVED ===")
        print("📨 \(message)")
        print("==============================\n")

        handleCastResult(message)
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: (any Error)?) {
        if let error = error {
            print("❌ Write failed: \(error)")
        } else {
            print("✏️  Cast trigger written - waiting for result...")
        }
    }

    // MARK: - Triggering a cast

    /// Any payload works - the board doesn't inspect what's written, only
    /// that a write happened. A single byte is enough.
    func triggerCast() {
        guard let peripheral = discoveredPeripheral, let characteristic = targetCharacteristic else {
            print("⚠️  Not connected to \(DEVICE_NAME) yet.")
            return
        }
        peripheral.writeValue(Data([1]), for: characteristic, type: .withResponse)
    }

    // MARK: - Handling a real cast result

    func handleCastResult(_ message: String) {
        guard let data = message.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] else {
            print("⚠️  Notify payload wasn't the expected JSON shape - ignoring.")
            return
        }

        let success = json["success"] as? Bool ?? false
        let spell = json["spell"] as? String ?? "?"
        print(success ? "✅ \(spell) resolved successfully" : "❌ \(spell) fizzled")

        if success {
            notifyDemoServer()
        }
    }

    /// Best-effort, mirrors the firmware's own notifyDemoServer() - never
    /// blocks or fails the actual BLE flow if the demo display isn't running.
    func notifyDemoServer() {
        guard let url = URL(string: DEMO_SERVER_URL) else { return }
        var request = URLRequest(url: url)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = Data("{}".utf8)

        URLSession.shared.dataTask(with: request) { _, response, error in
            if let error = error {
                print("ℹ️  Demo server not reachable (\(error.localizedDescription)) - skipping, this is optional.")
                return
            }
            if let httpResponse = response as? HTTPURLResponse {
                print("📺 Demo server notified -> HTTP \(httpResponse.statusCode)")
            }
        }.resume()
    }
}

// MARK: - Main

print("""
================================================
🔮 MAGIC BLE Bridge
================================================
BLE Device: MAGIC-ProS3
================================================

Starting bridge...

""")

let bridge = BLEBridge()

// Simple stdin command loop so a human at this Mac can trigger a cast
// without any MAGIC-aware code of their own - "cast" + Enter writes a
// trigger byte to the board, same as pressing its BOOT button.
let stdinQueue = DispatchQueue(label: "ble-bridge-stdin")
stdinQueue.async {
    while let line = readLine() {
        let command = line.trimmingCharacters(in: .whitespacesAndNewlines).lowercased()
        if command == "cast" {
            DispatchQueue.main.async {
                bridge.triggerCast()
            }
        } else if !command.isEmpty {
            print("Unknown command \"\(command)\" - only \"cast\" is supported.")
        }
    }
}

RunLoop.main.run()
