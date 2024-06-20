//
//  MPCBroadcaster.swift
//  Braintree
//
//  Created by Zach Babb on 6/28/20.
//

import Foundation
import MultipeerConnectivity

internal struct Payload {
    internal let message: String
    
    internal func toData() -> Data {
        return Data(message.utf8)
    }
}

class MPCBrowser: NSObject {
    
    var mcSession: MCSession?
    var mcBrowser: MCNearbyServiceBrowser?
    var mcPeerID: MCPeerID!
    
    init(peerID: String) {
        self.mcPeerID = MCPeerID(displayName: peerID)
        
        super.init()
        
        setupCallingSession()
    }
    
    func setupCallingSession() {
        mcSession = MCSession(peer: mcPeerID, securityIdentity: nil, encryptionPreference: .required)
        mcSession?.delegate = self
        
        mcBrowser = MCNearbyServiceBrowser(peer: mcPeerID, serviceType: "planet-nine")
        mcBrowser?.delegate = self
        mcBrowser?.startBrowsingForPeers()
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

extension MPCBrowser: MCSessionDelegate {
    func session(_ session: MCSession, peer peerID: MCPeerID, didChange state: MCSessionState) {
        
        if state == .connected {
            let payload = Payload(message: "Heyooo")
            sendMessage(payload)
        }
    }
    
    func session(_ session: MCSession, didReceive data: Data, fromPeer peerID: MCPeerID) {
        
    }
    
    func session(_ session: MCSession, didReceive stream: InputStream, withName streamName: String, fromPeer peerID: MCPeerID) {
        
    }
    
    func session(_ session: MCSession, didStartReceivingResourceWithName resourceName: String, fromPeer peerID: MCPeerID, with progress: Progress) {
        
    }
    
    func session(_ session: MCSession, didFinishReceivingResourceWithName resourceName: String, fromPeer peerID: MCPeerID, at localURL: URL?, withError error: Error?) {
        
    }
}

extension MPCBrowser: MCNearbyServiceBrowserDelegate {
    func browser(_ browser: MCNearbyServiceBrowser, foundPeer peerID: MCPeerID, withDiscoveryInfo info: [String : String]?) {
        guard let mcSession = mcSession else { return }
        
        browser.invitePeer(peerID, to: mcSession, withContext: nil, timeout: 1000)
    }
    
    func browser(_ browser: MCNearbyServiceBrowser, lostPeer peerID: MCPeerID) {
        
    }
}
