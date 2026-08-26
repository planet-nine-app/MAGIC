import Foundation

/// Self-registers this app's own Sessionless identity with fount, the same
/// bootstrap every other gateway in this repo does (espnow-target,
/// ble-sessionless, obs-gateway) - PUT /user/create with
/// {timestamp, pubKey, signature over timestamp+pubKey}.
final class FountClient {
    private let sessionless: MagicSessionless
    private(set) var uuid: String?
    private var ordinal: Int

    private static let uuidDefaultsKey = "magictv.uuid"
    private static let ordinalDefaultsKey = "magictv.ordinal"

    init(sessionless: MagicSessionless) {
        self.sessionless = sessionless
        self.uuid = UserDefaults.standard.string(forKey: Self.uuidDefaultsKey)
        self.ordinal = UserDefaults.standard.integer(forKey: Self.ordinalDefaultsKey)
    }

    /// A live counter, bumped by GatewayServer after each relayed cast - not
    /// strictly enforced by fount today (see obs-gateway's server.js for the
    /// same note), but kept correct anyway rather than reusing one value.
    func nextOrdinal() -> Int {
        ordinal += 1
        UserDefaults.standard.set(ordinal, forKey: Self.ordinalDefaultsKey)
        return ordinal
    }

    func registerIfNeeded() async -> String? {
        if let uuid { return uuid }

        let timestamp = String(Int(Date().timeIntervalSince1970 * 1000))
        let message = timestamp + sessionless.keys.publicKey
        let signature = sessionless.sign(message)

        guard let url = URL(string: "\(Config.fountBase)/user/create") else { return nil }
        var request = URLRequest(url: url)
        request.httpMethod = "PUT" // fount registers this route as app.put, not app.post
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try? JSONSerialization.data(withJSONObject: [
            "timestamp": timestamp,
            "pubKey": sessionless.keys.publicKey,
            "signature": signature
        ])

        do {
            let (data, response) = try await URLSession.shared.data(for: request)
            guard let http = response as? HTTPURLResponse, http.statusCode == 200,
                  let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
                  let newUUID = json["uuid"] as? String else {
                NSLog("FountClient: registration failed - is fount running at \(Config.fountBase)?")
                return nil
            }
            uuid = newUUID
            ordinal = (json["ordinal"] as? Int) ?? 0
            UserDefaults.standard.set(newUUID, forKey: Self.uuidDefaultsKey)
            UserDefaults.standard.set(ordinal, forKey: Self.ordinalDefaultsKey)
            return newUUID
        } catch {
            NSLog("FountClient: registration request failed: \(error.localizedDescription)")
            return nil
        }
    }
}
