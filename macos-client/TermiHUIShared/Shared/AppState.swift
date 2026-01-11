import Foundation

enum AppState {
    case welcome
    case connecting(serverAddress: String)
    case connected(serverAddress: String)
    case error(message: String)
}
