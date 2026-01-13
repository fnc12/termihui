import Foundation

/// Состояния приложения для управления навигацией
enum AppState {
    case welcome
    case connecting(serverAddress: String)
    case connected(serverAddress: String)
    case error(message: String)
}
