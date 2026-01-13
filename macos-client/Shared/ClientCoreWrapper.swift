import Foundation

/// Swift wrapper for TermiHUI C++ client core
class ClientCoreWrapper {
    
    /// Check if core is initialized
    var isInitialized: Bool {
        return termihui_is_initialized()
    }
    
    /// Get client core version
    static var version: String {
        guard let cString = termihui_get_version() else {
            return "unknown"
        }
        return String(cString: cString)
    }
    
    init() {
        print("🔗 ClientCoreWrapper: Created")
    }
    
    /// Initialize client core
    /// @return true if successful
    func initialize() -> Bool {
        print("🔗 ClientCoreWrapper: Initializing...")
        let result = termihui_initialize()
        
        if result {
            print("✅ ClientCoreWrapper: Initialized successfully")
        } else {
            print("❌ ClientCoreWrapper: Initialization failed")
        }
        
        return result
    }
    
    /// Shutdown client core
    func shutdown() {
        print("🔗 ClientCoreWrapper: Shutting down...")
        termihui_shutdown()
    }
    
    /// Send message to client core for processing
    /// @param message JSON or other encoded message
    /// @return response string (JSON)
    @discardableResult
    func sendMessage(_ message: String) -> String {
        guard let response = termihui_send_message(message) else {
            return #"{"error":"null_response"}"#
        }
        
        return String(cString: response)
    }
    
    /// Send message as dictionary (auto-encodes to JSON)
    /// @param dict dictionary to send
    @discardableResult
    func send(_ dict: [String: Any]) -> String {
        guard let data = try? JSONSerialization.data(withJSONObject: dict),
              let json = String(data: data, encoding: .utf8) else {
            print("❌ ClientCoreWrapper: Failed to encode message: \(dict)")
            return #"{"error":"encoding_failed"}"#
        }
        return sendMessage(json)
    }
    
    /// Get count of pending events
    var pendingEventsCount: Int {
        return Int(termihui_pending_events_count())
    }
    
    /// Poll single event from client core
    /// @return event string (JSON) or nil if no more events
    func pollEvent() -> String? {
        guard let event = termihui_poll_event() else {
            return nil
        }
        return String(cString: event)
    }
    
    /// Poll all pending events from client core
    /// @return array of event strings (JSON encoded)
    func pollAllEvents() -> [String] {
        var events: [String] = []
        while let event = pollEvent() {
            events.append(event)
        }
        return events
    }
    
    /// Update tick - process WebSocket events on main thread
    /// Call this regularly (e.g. 60fps) before pollEvent()
    func update() {
        termihui_update()
    }
}
