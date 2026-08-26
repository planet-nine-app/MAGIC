import SwiftUI

@main
struct MagicTVApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        WindowGroup {
            ContentView(model: model)
        }
    }
}

@MainActor
final class AppModel: ObservableObject {
    @Published var statusText = "Booting..."
    @Published var showPotionFlash = false

    let sessionless = MagicSessionless()
    private(set) lazy var fount = FountClient(sessionless: sessionless)
    private var gatewayServer: GatewayServer?
    private var started = false

    func start() {
        guard !started else { return }
        started = true

        gatewayServer = GatewayServer(sessionless: sessionless, fount: fount) { [weak self] success in
            Task { @MainActor in
                self?.flashPotion(success: success)
            }
        }

        Task {
            statusText = "Registering with fount..."
            let uuid = await fount.registerIfNeeded()

            if let uuid {
                statusText = "Ready. uuid=\(String(uuid.prefix(8)))... listening on :\(Config.listenPort)"
            } else {
                statusText = "fount registration failed - is it running at \(Config.fountBase)?"
            }

            do {
                try gatewayServer?.start()
            } catch {
                statusText = "GatewayServer failed to start: \(error.localizedDescription)"
            }
        }
    }

    private func flashPotion(success: Bool) {
        statusText = success ? "\u{2728} \(Config.spellName) relayed successfully" : "\u{274C} \(Config.spellName) fizzled"
        showPotionFlash = true
        Task {
            try? await Task.sleep(nanoseconds: 2_000_000_000)
            showPotionFlash = false
        }
    }
}
