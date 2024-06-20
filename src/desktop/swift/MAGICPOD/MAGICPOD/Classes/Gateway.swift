import Foundation
import Sessionless

public struct GatewayAddition: Codable {
    let timestamp: String
    let gatewayUUID: String
    let gatewayName: String
    let minimumCost: Int
    let ordinal: Int
    var gatewaySignature: String
    let additions: [String]
    
    init(timestamp: String, gatewayUUID: String, gatewayName: String, minimumCost: Int, ordinal: Int, additions: [String]) {
        self.timestamp = timestamp
        self.gatewayUUID = gatewayUUID
        self.gatewayName = gatewayName
        self.minimumCost = minimumCost
        self.ordinal = ordinal
        self.gatewaySignature = ""
        self.additions = additions
        
        self.sign()
    }
    
    func toString() -> String {
        return """
            {"timestamp":"\(timestamp)","gatewayUUID":"\(gatewayUUID)","gatewayName":"\(gatewayName)","minimumCost":\(minimumCost),"ordinal":"\(ordinal)","additions":"\(additions.joined())"}
        """
    }
    
    mutating func sign() {
        let sessionless = Sessionless()
        guard let signature = sessionless.sign(message: self.toString()) else { return }
        self.gatewaySignature = signature
    }
}

public struct GatewayMVP: Codable {
    let timestamp: String
    let uuid: String
    let minimumCost: Int
    let ordinal: Int
    var signature: String
    
    init(timestamp: String, uuid: String, minimumCost: Int, ordinal: Int) {
        self.timestamp = timestamp
        self.uuid = uuid
        self.minimumCost = minimumCost
        self.ordinal = ordinal
        self.signature = ""
        
        self.sign()
    }
    
    func toString() -> String {
        let message = """
        {"timestamp":"\(timestamp)","uuid":"\(uuid)","minimumCost":\(minimumCost),"ordinal":\(ordinal)}
        """
        print("gateway message: \(message)")
        return message
    }
    
    func toSpellString() -> String {
        return """
        {"timestamp":"\(timestamp)","uuid":"\(uuid)","minimumCost":\(minimumCost),"ordinal":\(ordinal),\"signature\":\"\(signature)\"}
        """
    }
    
    mutating func sign() {
        let sessionless = Sessionless()
        guard let signature = sessionless.sign(message: self.toString()) else { return }
        self.signature = signature
    }
}

public struct Spell: Codable {
    var timestamp: String = ""
    var spellName: String = ""
    var casterUUID: String = ""
    var totalCost: Int = 1
    var mp: Bool = false
    var ordinal: Int = 1
    var casterSignature: String = ""
    var gateways: [GatewayMVP] = []
    var additions: [String] = []
    
    init() {
        
    }
    
    init(timestamp: String, spellName: String, casterUUID: String, totalCost: Int, mp: Bool, ordinal: Int, casterSignature: String, gateways: [GatewayMVP], additions: [String]) {
        self.timestamp = timestamp
        self.spellName = spellName
        self.casterUUID = casterUUID
        self.totalCost = totalCost
        self.mp = mp
        self.ordinal = ordinal
        self.casterSignature = casterSignature
        self.gateways = gateways
        self.additions = additions
        
        //self.sign()

    }
    
    init(spellData: Data) {
        var decodedSpell = Spell()
        do {
            decodedSpell = try JSONDecoder().decode(Spell.self, from: spellData)
        } catch {
            print("Error getting gateway")
            print(error)
        }
        self = decodedSpell
    }
    
    public func toString() -> String {
        var gatewaysMapped = gateways.map { gateway in
            return "\(gateway.toSpellString()),"
        }
        var gatewaysAsStrings = gatewaysMapped.joined()
        gatewaysAsStrings.popLast()
        let gatewaysAsStringsArray = "[\(gatewaysAsStrings)]"
        
        print(gatewaysAsStringsArray)
        
        let spellString = """
            {"timestamp":"\(timestamp)","spell":"\(spellName)","casterUUID":"\(casterUUID)","totalCost":\(totalCost),"mp":\(1),"ordinal":\(ordinal),"gateways":\(gatewaysAsStringsArray),"additions":"\(additions.joined())","casterSignature":"\(casterSignature)"}
        """
        
        print(spellString)
        return spellString
    }
    
    public func toMessageString() -> String {
        let spellString = """
        {"timestamp":"\(timestamp)","spell":"\(spellName)","casterUUID":"\(casterUUID)","totalCost":\(totalCost),"mp":\(1),"ordinal":"\(ordinal)"}
        """
        
        return spellString
    }
    
    mutating func sign() {
        let sessionless = Sessionless()
        guard let signature = sessionless.sign(message: self.toMessageString()) else { return }
        self.casterSignature = signature
    }
}

public struct User: Codable {
    let uuid: String
}

public enum Transport: String {
    case ble = "ble"
    case mpc = "mpc"
    case http = "http"
}

public class Gateway {
    private var bleMAGICPeripheral: BLEMAGICPeripheral!
    private let sessionless = Sessionless()
    private var uuid: String!
    
    public init(incomingTransports: [Transport], forwardSpell: @escaping (Spell) -> Void, uiCallback: @escaping (String) -> Void) async {
        //if sessionless.getKeys() == nil {
        if true {
            print("no keys")
            //let keys = sessionless.generateKeys()
            let keys = sessionless.getKeys()
            let pubKey = keys?.publicKey ?? ""
            print("pubKey: \(pubKey)")
            let message = "{\"timestamp\":\"rightnow\",\"pubKey\":\"\(pubKey)\"}"
            print("message: \(message)")
            let signature = sessionless.sign(message: message) ?? ""
            let hash = "first-hash"
            
            print("signature: \(signature)")
            print("test of reg veg \(sessionless.verifySignature(signature: signature, message: message, publicKey: pubKey))")
            let registration = Registration(timestamp: "rightnow", pubKey: pubKey, signature: signature, hash: hash)
            
            Task {
                await Network.register(payload: registration) { [weak self] error, data in
                    if let data = data {
                        print("registration complete")
                        print(data)
                        do {
                            print(String(data: data, encoding: .utf8))
                            let user = try JSONDecoder().decode(User.self, from: data)
                            print(user)
                            self?.uuid = user.uuid
                            print(user.uuid)
                        } catch {
                            print(error)
                        }
                    }
                }
            }
        }
        
        incomingTransports.forEach { transport in
            switch transport {
            case .ble: startBLEMAGICPeripheral(forwardSpell: forwardSpell, uiCallback: uiCallback)
            default: print("unimplemented")
            }
        }
    }
    
    private func startBLEMAGICPeripheral(forwardSpell: @escaping (Spell) -> Void, uiCallback: @escaping (String) -> Void) {
        
        bleMAGICPeripheral = BLEMAGICPeripheral { [weak self] spell in
            guard let self = self else { return }
            let sessionless = Sessionless()
            /*let gatewayAddition = GatewayAddition(timestamp: "right now", gatewayUUID: , gatewayName: "Bar", minimumCost: 40, ordinal: 1, additions: [])*/
            let gatewayMVP = GatewayMVP(timestamp: "right now", uuid: self.uuid, minimumCost: 50, ordinal: 2)
            guard let newSpell = self.addToSpell(spell: spell, gatewayMVP: gatewayMVP) else { return }
            print(newSpell)
            uiCallback(gatewayMVP.toString())
            // Send new spell here
            forwardSpell(newSpell)
        }
    }
    
    private func addToSpell(spell: Spell, gatewayMVP: GatewayMVP) -> Spell? {
        var newSpell = spell
        newSpell.gateways.append(gatewayMVP)
        return newSpell
    }
}
