import Foundation

/// Chat message model
struct ChatMessage {
    enum Role {
        case user
        case assistant
    }
    
    var role: Role
    var content: String
    var isStreaming: Bool = false
}
