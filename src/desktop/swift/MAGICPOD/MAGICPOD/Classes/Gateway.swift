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
    
    init(timestamp: String, gatewayUUID: String, gatewayName: String, minimumCost: Int, ordinal: Int, gatewaySignature: String, additions: [String]) {
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

public struct Spell: Codable {
    var timestamp: String = ""
    var spellName: String = ""
    var casterUUID: String = ""
    var totalCost: Int = 1
    var mp: Bool = false
    var ordinal: Int = 1
    var casterSignature: String = ""
    var gateways: [GatewayAddition] = []
    var additions: [String] = []
    
    init() {
        
    }
    
    init(timestamp: String, spellName: String, casterUUID: String, totalCost: Int, mp: Bool, ordinal: Int, gateways: [GatewayAddition], additions: [String]) {
        self.timestamp = timestamp
        self.spellName = spellName
        self.casterUUID = casterUUID
        self.totalCost = totalCost
        self.mp = mp
        self.ordinal = ordinal
        self.casterSignature = ""
        self.gateways = gateways
        self.additions = additions
        
        self.sign()

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
    
    func toString() -> String {
        return """
            {"timestamp":"\(timestamp)","spellName":"\(spellName)","casterUUID":"\(casterUUID)","totalCost":\(totalCost),"mp":\(mp),"ordinal":"\(ordinal)","additions":"\(additions.joined())"}
        """
    }
    
    mutating func sign() {
        let sessionless = Sessionless()
        guard let signature = sessionless.sign(message: self.toString()) else { return }
        self.casterSignature = signature
    }
}

public class Gateway {
    
    func addToSpell(spell: Spell, gatewayAddition: GatewayAddition) -> Spell? {
        var newSpell = spell
        newSpell.gateways.append(gatewayAddition)
        return newSpell
    }
}
