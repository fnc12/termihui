import Foundation

/// Session information received from server
struct SessionInfo: Codable {
    let id: UInt64
    let createdAt: Int64
    
    enum CodingKeys: String, CodingKey {
        case id
        case createdAt = "created_at"
    }
}
