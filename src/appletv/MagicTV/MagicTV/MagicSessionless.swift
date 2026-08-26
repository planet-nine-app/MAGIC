import Foundation
import JavaScriptCore
import Security

/// Sessionless signing via a corrected, from-scratch crypto.js (see
/// ../crypto-bundle/entry.js) run inside JavaScriptCore. NOT built on either
/// existing vendored crypto.js in this ecosystem - both have real signing
/// bugs (see crypto-bundle/entry.js's header comment for the specifics).
/// This one is cross-verified against the real sessionless-node in both
/// directions (see this session's transcript / crypto-bundle/entry.js).
final class MagicSessionless {
    struct Keys {
        let publicKey: String
        let privateKey: String
    }

    private var generateKeysJS: JSValue?
    private var signJS: JSValue?
    private var verifyJS: JSValue?

    private let keyService = "MagicTVKeyStore"
    private let keyAccount = "MagicTV"

    let keys: Keys

    init() {
        let context = Self.loadContext()
        let bridge = context.objectForKeyedSubscript("globalThis").objectForKeyedSubscript("sessionless")
        generateKeysJS = bridge?.objectForKeyedSubscript("generateKeys")
        signJS = bridge?.objectForKeyedSubscript("sign")
        verifyJS = bridge?.objectForKeyedSubscript("verifySignature")

        if let existing = Self.readKeysFromKeychain(service: keyService, account: keyAccount) {
            keys = existing
        } else {
            var randomBytes = [UInt8](repeating: 0, count: 32)
            let status = SecRandomCopyBytes(kSecRandomDefault, randomBytes.count, &randomBytes)
            precondition(status == errSecSuccess, "SecRandomCopyBytes failed")
            let privateKeyHex = Data(randomBytes).hexEncodedString()

            guard let generated = generateKeysJS?.call(withArguments: [privateKeyHex]),
                  let publicKey = generated.objectForKeyedSubscript("publicKey")?.toString(),
                  let privateKey = generated.objectForKeyedSubscript("privateKey")?.toString() else {
                fatalError("MagicSessionless: key generation failed - check crypto.js is bundled correctly")
            }

            let newKeys = Keys(publicKey: publicKey, privateKey: privateKey)
            Self.saveKeysToKeychain(newKeys, service: keyService, account: keyAccount)
            keys = newKeys
        }
    }

    func sign(_ message: String) -> String {
        signJS?.call(withArguments: [message, keys.privateKey])?.toString() ?? ""
    }

    func verify(signature: String, message: String, publicKey: String) -> Bool {
        verifyJS?.call(withArguments: [signature, message, publicKey])?.toBool() ?? false
    }

    private static func loadContext() -> JSContext {
        guard let url = Bundle.main.url(forResource: "crypto", withExtension: "js"),
              let source = try? String(contentsOf: url, encoding: .utf8) else {
            fatalError("crypto.js missing from app bundle - check project.yml's Resources build phase")
        }
        let context = JSContext()!
        context.exceptionHandler = { _, exception in
            NSLog("MagicSessionless JS error: %@", exception?.toString() ?? "?")
        }
        context.evaluateScript(source)
        return context
    }

    private static func readKeysFromKeychain(service: String, account: String) -> Keys? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecReturnData as String: true
        ]
        var result: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &result) == errSecSuccess,
              let data = result as? Data,
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: String],
              let publicKey = json["publicKey"], let privateKey = json["privateKey"] else {
            return nil
        }
        return Keys(publicKey: publicKey, privateKey: privateKey)
    }

    private static func saveKeysToKeychain(_ keys: Keys, service: String, account: String) {
        guard let data = try? JSONSerialization.data(withJSONObject: ["publicKey": keys.publicKey, "privateKey": keys.privateKey]) else {
            return
        }
        let deleteQuery: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account
        ]
        SecItemDelete(deleteQuery as CFDictionary)

        let addQuery: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: account,
            kSecValueData as String: data
        ]
        SecItemAdd(addQuery as CFDictionary, nil)
    }
}

extension Data {
    func hexEncodedString() -> String {
        map { String(format: "%02x", $0) }.joined()
    }
}
