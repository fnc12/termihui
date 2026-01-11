import Foundation

class AppSettings {
    private let userDefaults = UserDefaults.standard
    private let serverAddressKey = "serverAddress"

    var serverAddress: String {
        get {
            return userDefaults.string(forKey: serverAddressKey) ?? "localhost:37854"
        }
        set {
            userDefaults.set(newValue, forKey: serverAddressKey)
        }
    }

    var hasServerAddress: Bool {
        return userDefaults.string(forKey: serverAddressKey) != nil
    }

    static let shared = AppSettings()
    private init() {}
}
