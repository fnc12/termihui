import Foundation

/// Application settings
class AppSettings {
    private let userDefaults = UserDefaults.standard
    private let serverAddressKey = "serverAddress"
    private let serversKey = "servers"
    
    /// Saved server address or default value (for macOS)
    var serverAddress: String {
        get {
            return userDefaults.string(forKey: serverAddressKey) ?? "localhost:37854"
        }
        set {
            userDefaults.set(newValue, forKey: serverAddressKey)
        }
    }
    
    /// Checks if there is a saved server address
    var hasServerAddress: Bool {
        return userDefaults.string(forKey: serverAddressKey) != nil
    }
    
    // MARK: - Servers List (iOS)
    
    /// List of saved servers
    var servers: [String] {
        get {
            return userDefaults.stringArray(forKey: serversKey) ?? []
        }
        set {
            userDefaults.set(newValue, forKey: serversKey)
        }
    }
    
    /// Add server to list
    func addServer(_ address: String) {
        var current = servers
        // Don't add duplicates
        if !current.contains(address) {
            current.append(address)
            servers = current
        }
    }
    
    /// Remove server from list
    func removeServer(at index: Int) {
        var current = servers
        guard index >= 0 && index < current.count else { return }
        current.remove(at: index)
        servers = current
    }
    
    static let shared = AppSettings()
    private init() {}
}
