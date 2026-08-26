import SwiftUI
import AVKit

struct ContentView: View {
    @ObservedObject var model: AppModel

    var body: some View {
        ZStack {
            VideoPlayer(player: AVPlayer(url: Config.hlsURL))
                .ignoresSafeArea()
                .onAppear { model.start() }

            if model.showPotionFlash {
                Image(systemName: "flame.fill")
                    .resizable()
                    .scaledToFit()
                    .frame(width: 160, height: 160)
                    .foregroundColor(.pink)
                    .shadow(radius: 20)
                    .transition(.scale.combined(with: .opacity))
                    .animation(.spring(), value: model.showPotionFlash)
            }

            VStack {
                Spacer()
                Text(model.statusText)
                    .font(.caption)
                    .padding(10)
                    .background(.black.opacity(0.6))
                    .foregroundColor(.white)
                    .cornerRadius(8)
                    .padding()
            }
        }
    }
}
