import Foundation

struct LLMProvider: Codable {
    var id: UInt64
    var name: String
    var type: String
    var url: String
    var model: String
    var createdAt: Int64
    
    enum CodingKeys: String, CodingKey {
        case id, name, type, url, model
        case createdAt = "created_at"
    }
}
