import Foundation
import Swifter

/// This app's half of the MAGIC gateway relay pattern - the Swift
/// equivalent of magic-gateway-js's gateway.expressApp() used by every
/// other Node hop in this demo chain (../../streaming/streaming-server/,
/// ../../streaming/obs-gateway/, ../../games/potion-game/). Receives a
/// spell from A (the caster), adds this app's own gateway signature, and
/// forwards to C (the streaming server) - see ../../streaming/README.md for
/// the full chain and why each hop's next-destination is hardcoded locally
/// (Config.nextHopURL) rather than read from a shared spellbook file the
/// way the Node hops do.
final class GatewayServer {
    private let server = HttpServer()
    private let sessionless: MagicSessionless
    private let fount: FountClient
    private let onCastRelayed: (Bool) -> Void

    init(sessionless: MagicSessionless, fount: FountClient, onCastRelayed: @escaping (Bool) -> Void) {
        self.sessionless = sessionless
        self.fount = fount
        self.onCastRelayed = onCastRelayed
    }

    func start() throws {
        server["/magic/spell/\(Config.spellName)"] = { [weak self] request in
            guard let self else { return .internalServerError }
            return self.handleCast(bodyBytes: request.body)
        }
        try server.start(Config.listenPort, forceIPv4: false)
        NSLog("GatewayServer listening on :\(Config.listenPort)")
    }

    private func handleCast(bodyBytes: [UInt8]) -> HttpResponse {
        guard let spell = try? JSONSerialization.jsonObject(with: Data(bodyBytes)) as? [String: Any] else {
            return .badRequest(.text("invalid JSON"))
        }
        guard let ownUUID = fount.uuid else {
            return HttpResponse.raw(503, "Service Unavailable", nil) { writer in
                try writer.write("not registered with fount yet".data(using: .utf8)!)
            }
        }

        var relayedSpell = spell
        var gateways = (spell["gateways"] as? [[String: Any]]) ?? []

        let timestamp = String(Int(Date().timeIntervalSince1970 * 1000))
        let minimumCost = 0
        let ordinal = fount.nextOrdinal()
        // Must match fount's resolve() reconstruction exactly:
        // gateway.timestamp + gateway.uuid + gateway.minimumCost + gateway.ordinal
        let message = timestamp + ownUUID + String(minimumCost) + String(ordinal)
        let signature = sessionless.sign(message)

        gateways.append([
            "timestamp": timestamp,
            "uuid": ownUUID,
            "minimumCost": minimumCost,
            "ordinal": ordinal,
            "signature": signature
        ])
        relayedSpell["gateways"] = gateways

        let (statusCode, responseBody) = forwardSynchronously(spell: relayedSpell)
        let success = (responseBody?["success"] as? Bool) ?? false
        onCastRelayed(success)

        guard let responseBody else {
            return .internalServerError
        }
        guard let responseData = try? JSONSerialization.data(withJSONObject: responseBody) else {
            return .internalServerError
        }
        return HttpResponse.raw(statusCode, "OK", ["Content-Type": "application/json"]) { writer in
            try writer.write(responseData)
        }
    }

    /// Swifter's route handlers are synchronous, so this blocks the request
    /// thread on the outbound relay - matches the deliberately simple,
    /// blocking style the rest of this demo chain uses (the ESP32 firmware
    /// blocks on its own HTTPS calls too) rather than adding async
    /// plumbing for a demo.
    private func forwardSynchronously(spell: [String: Any]) -> (Int, [String: Any]?) {
        var request = URLRequest(url: Config.nextHopURL)
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONSerialization.data(withJSONObject: spell)

        let semaphore = DispatchSemaphore(value: 0)
        var statusCode = 502
        var responseBody: [String: Any]?

        let task = URLSession.shared.dataTask(with: request) { data, response, error in
            if let error {
                NSLog("GatewayServer: forward to \(Config.nextHopURL) failed: \(error.localizedDescription)")
            } else if let http = response as? HTTPURLResponse {
                statusCode = http.statusCode
                if let data, let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any] {
                    responseBody = json
                }
            }
            semaphore.signal()
        }
        task.resume()
        _ = semaphore.wait(timeout: .now() + 15)

        return (statusCode, responseBody)
    }
}
