import Foundation

/// Настройки приложения
class AppSettings {
    private let userDefaults = UserDefaults.standard
    private let serverAddressKey = "serverAddress"
    
    /// Сохраненный адрес сервера или значение по умолчанию
    var serverAddress: String {
        get {
            return userDefaults.string(forKey: serverAddressKey) ?? "localhost:37854"
        }
        set {
            userDefaults.set(newValue, forKey: serverAddressKey)
        }
    }
    
    /// Проверяет, есть ли сохраненный адрес сервера
    var hasServerAddress: Bool {
        return userDefaults.string(forKey: serverAddressKey) != nil
    }
    
    static let shared = AppSettings()
    private init() {}
}
