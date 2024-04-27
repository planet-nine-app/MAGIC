//
//  MAGICGateway.swift
//  PlanetNineGateway
//
//  Created by Zach Babb on 2/6/19.
//  Copyright © 2019 Planet Nine. All rights reserved.
//

import Foundation
import CoreBluetooth

class BLEMAGICTransport {
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
        return ""
    }
    
    func writeCallback(value: String, central: CBCentral) {
        if value == "start spell" {
            shouldListenForSpell = true
            incomingSpell = Data()
        } else {
            shouldListenForSpell = false
            
        }
    }
    
    func notifyCallback(characteristic: CBCharacteristic) -> String {
        guard let value = characteristic.value else { return "" }
        if shouldListenForSpell {
            incomingSpell.append(value)
        }
        return ""
    }
    
}
