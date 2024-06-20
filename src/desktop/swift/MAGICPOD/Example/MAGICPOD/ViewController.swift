//
//  ViewController.swift
//  MAGICPOD
//
//  Created by zach-planet-nine on 04/24/2024.
//  Copyright (c) 2024 zach-planet-nine. All rights reserved.
//

import Cocoa
import MAGICPOD

class ViewController: NSViewController {
    
    var gateway: Gateway!

  override func viewDidLoad() {
    super.viewDidLoad()

      Task {
          gateway = await Gateway(incomingTransports: [.ble], forwardSpell: forwardSpell(_ :)) { [weak self] message in
              if message != nil {
                  self?.view.layer?.backgroundColor = CGColor(red: 0, green: 200, blue: 25, alpha: 1)
              }
          }
      }
      
    // Do any additional setup after loading the view.
  }
    
    func forwardSpell(_ spell: Spell) {
        Task {
            await Network.forwardSpell(payload: spell) { [weak self] error, data in
                if let error = error {
                    print(error)
                    print("erorororo")
                    return
                }
                if let data = data {
                    print(String(data: data, encoding: .utf8))
                }
            }
        }
    }

  override var representedObject: Any? {
    didSet {
    // Update the view, if already loaded.
    }
  }


}

