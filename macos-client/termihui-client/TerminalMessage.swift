import Foundation

/// Сообщения, отправляемые на сервер
enum TerminalMessage: Encodable {
    case execute(command: String)
    case input(text: String)
    case completion(text: String, cursorPosition: Int)
    
    private enum CodingKeys: String, CodingKey {
        case type, command, text, cursor_position
    }
    
    func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        
        switch self {
        case .execute(let command):
            try container.encode("execute", forKey: .type)
            try container.encode(command, forKey: .command)
        case .input(let text):
            try container.encode("input", forKey: .type)
            try container.encode(text, forKey: .text)
        case .completion(let text, let cursorPosition):
            try container.encode("completion", forKey: .type)
            try container.encode(text, forKey: .text)
            try container.encode(cursorPosition, forKey: .cursor_position)
        }
    }
}

/// Ответы, получаемые от сервера
struct TerminalResponse: Codable {
    let type: String
    let data: String?
    let running: Bool?
    let exitCode: Int?
    let message: String?
    let errorCode: String?
    let serverVersion: String?
    let bytes: Int?
    
    // Поля для completion_result
    let completions: [String]?
    let originalText: String?
    let cursorPosition: Int?
    
    private enum CodingKeys: String, CodingKey {
        case type, data, running, message, completions
        case exitCode = "exit_code"
        case errorCode = "error_code"
        case serverVersion = "server_version"
        case bytes
        case originalText = "original_text"
        case cursorPosition = "cursor_position"
    }
}
