//
//  BLEUtils.swift
//  BLETester
//
//  Created by Zach Babb on 12/13/18.
//  Copyright © 2018 Planet Nine. All rights reserved.
//

import Foundation
import CoreBluetooth

struct BLEServices {
    let twoWay = CBUUID(string: "5995AB90-709A-4735-AAF2-DF2C8B061BB4")
}

struct BLECharacteristics {
    let readMagicGateway = CBUUID(string: "3558E2EC-BF6C-41F0-BC9F-EBB51B8C87CE")
    let readMagicDevice = CBUUID(string: "2D98DB2F-C78D-4F15-AE30-2185CC77469A")
    let write = CBUUID(string: "4D8D84E5-5889-4310-80BF-0D44DCB49762")
    let notify = CBUUID(string: "CD6984D2-5055-4033-A42E-BB039FC6EF6B")
}
