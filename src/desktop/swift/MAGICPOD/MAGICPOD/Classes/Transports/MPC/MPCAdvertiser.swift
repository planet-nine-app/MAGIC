//
//  MPCAdvertiser.swift
//  Braintree
//
//  Created by Zach Babb on 6/28/20.
//

import Foundation
import MultipeerConnectivity

class MPCAdvertiser: NSObject {
    
    var mcSession: MCSession?
    var mcAdvertiser: MCNearbyServiceAdvertiser?
    var mcPeerID: MCPeerID!
    
    init(peerID: String) {
        self.mcPeerID = MCPeerID(displayName: peerID)
        
        super.init()
        
        setupCallingSession()
    }
    
    func setupCallingSession() {
        mcSession = MCSession(peer: self.mcPeerID, securityIdentity: nil, encryptionPreference: .required)
        mcSession?.delegate = self
        
        mcAdvertiser = MCNearbyServiceAdvertiser(peer: mcPeerID, discoveryInfo: nil, serviceType: "planet-nine")
        mcAdvertiser?.delegate = self
        mcAdvertiser?.startAdvertisingPeer()
    }
    
    func sendMessage(_ payload: Payload) {
        guard let mcSession = mcSession,
              mcSession.connectedPeers.count > 0
        else { return }
        
        do {
            try mcSession.send(payload.toData(), toPeers: mcSession.connectedPeers, with: .unreliable)
            print("Data: sent \(payload.toData().count) bytes")
        }
        catch {
            print("Error sending message: \(error)")
        }
    }
}

extension MPCAdvertiser: MCSessionDelegate {
    func session(_ session: MCSession, peer peerID: MCPeerID, didChange state: MCSessionState) {
        
    }
    
    func session(_ session: MCSession, didReceive data: Data, fromPeer peerID: MCPeerID) {
        
        let message = String(data: data, encoding: .utf8)
        
        print("Received message: \(message)")
    }
    
    func session(_ session: MCSession, didReceive stream: InputStream, withName streamName: String, fromPeer peerID: MCPeerID) {
        
    }
    
    func session(_ session: MCSession, didStartReceivingResourceWithName resourceName: String, fromPeer peerID: MCPeerID, with progress: Progress) {
        
    }
    
    func session(_ session: MCSession, didFinishReceivingResourceWithName resourceName: String, fromPeer peerID: MCPeerID, at localURL: URL?, withError error: Error?) {
        
    }
    
    
}

extension MPCAdvertiser: MCNearbyServiceAdvertiserDelegate {
    func advertiser(_ advertiser: MCNearbyServiceAdvertiser, didReceiveInvitationFromPeer peerID: MCPeerID, withContext context: Data?, invitationHandler: @escaping (Bool, MCSession?) -> Void) {
        guard let mcSession = mcSession else { return }
        
        invitationHandler(true, mcSession)
    }
    
    
}
