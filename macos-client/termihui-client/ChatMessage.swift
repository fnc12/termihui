import Foundation

/// Chat message model for UI display
struct ChatMessage {
    enum Role {
        case user
        case assistant
    }
    
    var role: Role
    var content: String
    var isStreaming: Bool = false
}

/// Chat message info from server (for history loading)
struct ChatMessageInfo: Codable {
    let id: UInt64
    let role: String
    let content: String
    let createdAt: Int64
    
    enum CodingKeys: String, CodingKey {
        case id, role, content
        case createdAt = "created_at"
    }
}
