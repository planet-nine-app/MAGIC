//
//  BLETwoWayCentral.swift
//  Planet Nine
//
//  Created by Zach Babb on 12/13/18.
//  Copyright © 2018 Planet Nine. All rights reserved.
//

import Foundation
import CoreBluetooth

class BLETwoWayCentral: NSObject, CBCentralManagerDelegate, CBPeripheralDelegate {
    
    var manager: CBCentralManager?
    var peripherals: [CBPeripheral]?
    var readCharacteristic: CBCharacteristic?
    var writeCharacteristic: CBCharacteristic?
    var notifyCharacteristic: CBCharacteristic?
    var readCallback: ((String) -> Void)?
    var notifyCallback: ((String) -> Void)?
    
    override init() {
        super.init()
        manager = CBCentralManager(delegate: self, queue: nil, options: [CBCentralManagerOptionShowPowerAlertKey: false])
        peripherals = [CBPeripheral]()
    }
    
    init(readCallback: @escaping (String) -> Void, notifyCallback: @escaping (String) -> Void) {
        super.init()
        manager = CBCentralManager(delegate: self, queue: nil, options: [CBCentralManagerOptionShowPowerAlertKey: false])
        peripherals = [CBPeripheral]()
        self.readCallback = readCallback
        self.notifyCallback = notifyCallback
    }
    
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOff:
            print("off")
        case .poweredOn:
            print("Powered On")
            central.scanForPeripherals(withServices: [BLEServices().twoWay], options: nil)

        case .unauthorized:
            print("unauthorized")
        case .resetting:
            print("resetting")
        case .unknown:
            print("unknown")
        case .unsupported:
            print("unsupported")
        default:
            print("Got to default somehow")
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String : Any], rssi RSSI: NSNumber) {
        print("Discovered peripheral")
        print("\n\n")
        print(peripheral)
        print("\n\n")
        peripherals?.append(peripheral)
        manager?.connect(peripheral, options: nil)
    }
    
    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        print("Connected to peripheral")
        peripheral.delegate = self
        peripheral.discoverServices([BLEServices().twoWay])
        
        let _ = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: false) { _ in
            self.checkForConnectedPeripherals()
        }
    }
    
    func checkForConnectedPeripherals() {
        guard let peripherals = manager?.retrieveConnectedPeripherals(withServices: [BLEServices().twoWay]) else {
            
            return
        }
        if peripherals.count == 0 {
            
            return
        }
        let _ = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: false) { _ in
            self.checkForConnectedPeripherals()
        }
    }
    
    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        print("We got them services")
        peripheral.discoverCharacteristics([BLECharacteristics().readMagicGateway, BLECharacteristics().write, BLECharacteristics().notify], for: peripheral.services![0])
    }
    
    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        for characteristic in service.characteristics! {
            print(characteristic.uuid)
            switch characteristic.uuid {
            case BLECharacteristics().readMagicGateway:
                print("Read it!")
                readCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                break
            case BLECharacteristics().write:
                print("Write to it!")
                writeCharacteristic = characteristic
                //peripheral.writeValue("Heyooo".data(using: .utf8)!, for: characteristic, type: .withResponse)
                break
            case BLECharacteristics().notify:
                print("Subscribe to it!")
                notifyCharacteristic = characteristic
                peripheral.setNotifyValue(true, for: characteristic)
                break
            default:
                print("Got to the characteristics default somehow")
            }
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        print("You've got an updated value there buddy")
        guard let data = characteristic.value,
              let value = String(data: data, encoding: .utf8),
              let readCallback = readCallback else { return }
        print(value)
        print(characteristic.uuid)
        print(readCharacteristic?.uuid)
        if characteristic.uuid == readCharacteristic?.uuid {
            readCallback(value)
        } else if characteristic.uuid == notifyCharacteristic?.uuid {
            
            guard let notifyCallback = notifyCallback else { return }
            notifyCallback(value)
        }
    }
    
    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        print("Apparently you wrote the value")
        //peripheral.readValue(for: readCharacteristic!)
    }
    
    func respondToGateway(userUUID: String, ordinal: Int, timestamp: String, signature: String) {
        //let gatewayResponse = GatewayResponse(userUUID: userUUID, ordinal: ordinal, signature: signature, timestamp: timestamp, ongoing: true)
        /*guard let data = gatewayResponse.toString().data(using: .utf8),
              let writeCharacteristic = writeCharacteristic,
              let peripherals = peripherals else { return }
        if peripherals.count > 0 {
            let peripheral = peripherals[0]
            peripheral.writeValue(data, for: writeCharacteristic, type: .withResponse)
        }*/
    }
    
}
