import Foundation

/// Application states for navigation management
enum AppState {
    case welcome
    case connecting(serverAddress: String)
    case connected(serverAddress: String)
    case error(message: String)
}
