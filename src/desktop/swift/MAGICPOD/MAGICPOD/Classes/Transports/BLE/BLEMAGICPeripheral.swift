//
//  MAGICGateway.swift
//  PlanetNineGateway
//
//  Created by Zach Babb on 2/6/19.
//  Copyright © 2019 Planet Nine. All rights reserved.
//

import Foundation
import CoreBluetooth
import Sessionless

class BLEMAGICPeripheral {
    var twoWayPeripheral: BLETwoWayPeripheral!
    let spellReceivedCallback: (_ spell: Spell) -> Void
    let bleCharacteristics = BLECharacteristics()
    var shouldListenForSpell = false
    var incomingSpell: Data!
    
    init(spellReceivedCallback: @escaping (_ spell: Spell) -> Void) {
        twoWayPeripheral = BLETwoWayPeripheral()
        self.spellReceivedCallback = spellReceivedCallback
        twoWayPeripheral.readCallback = self.readCallback
        twoWayPeripheral.writeCallback = self.writeCallback
        twoWayPeripheral.notifyCallback = self.notifyCallback
    }
    
    func readCallback(characteristic: CBCharacteristic) -> String {
        // In a real implementation, you may use this to negotiate some sort of
        // sense that this gateway, and your caster are part of the same network
        print("got a read request")
        return "magic"
    }
    
    func writeCallback(value: String, central: CBCentral) {
        print("value written \(value)")
        let spellComponents = value.split(separator: ":")
        print(spellComponents)
        //let sessionless = Sessionless()
        //let verified = sessionless.verifySignature(signature: value, message: "Here is a message", publicKey: "03f60b3bf11552f5a0c7d6b52fcc415973d30b52ab1d74845f1b34ae8568a47b5f")
        //print(verified)
        if spellComponents.count < 6 {
            print("missing components")
            return
        }
        if value == "start spell" {
            shouldListenForSpell = true
            incomingSpell = Data()
        } else {
            print("mp: \(spellComponents[4])")
            print(String(spellComponents[6]))
            
            spellReceivedCallback(Spell(timestamp: String(spellComponents[0]), spellName: String(spellComponents[1]), casterUUID: String(spellComponents[2]), totalCost: Int(spellComponents[3]) ?? 0, mp: spellComponents[4] == "1", ordinal: Int(spellComponents[5]) ?? 0, casterSignature: String(spellComponents[6]), gateways: [], additions: []))
            shouldListenForSpell = false
        }
    }
    
    func notifyCallback(characteristic: CBCharacteristic) -> String {
        print("notified")
        guard let value = characteristic.value else { return "" }
        if shouldListenForSpell {
            incomingSpell.append(value)
        }
        return ""
    }
    
}
