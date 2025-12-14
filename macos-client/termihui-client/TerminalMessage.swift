import Foundation

/// Messages sent to server
enum TerminalMessage: Encodable {
    case execute(command: String)
    case input(text: String)
    case completion(text: String, cursorPosition: Int)
    case resize(cols: Int, rows: Int)
    
    private enum CodingKeys: String, CodingKey {
        case type, command, text, cursor_position, cols, rows
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
        case .resize(let cols, let rows):
            try container.encode("resize", forKey: .type)
            try container.encode(cols, forKey: .cols)
            try container.encode(rows, forKey: .rows)
        }
    }
}

/// Command history record
struct CommandHistoryRecord: Codable {
    let command: String
    let output: String
    let exitCode: Int
    let cwdStart: String
    let cwdEnd: String
    let isFinished: Bool
    
    private enum CodingKeys: String, CodingKey {
        case command, output
        case exitCode = "exit_code"
        case cwdStart = "cwd_start"
        case cwdEnd = "cwd_end"
        case isFinished = "is_finished"
    }
}

/// Responses received from server
struct TerminalResponse: Codable {
    let type: String
    let data: String?
    let running: Bool?
    let exitCode: Int?
    let message: String?
    let errorCode: String?
    let serverVersion: String?
    let bytes: Int?
    let command: String?
    let cwd: String?
    
    // Fields for completion_result
    let completions: [String]?
    let originalText: String?
    let cursorPosition: Int?
    
    // Fields for history
    let commands: [CommandHistoryRecord]?
    
    private enum CodingKeys: String, CodingKey {
        case type, data, running, message, completions, commands
        case exitCode = "exit_code"
        case errorCode = "error_code"
        case serverVersion = "server_version"
        case bytes
        case originalText = "original_text"
        case cursorPosition = "cursor_position"
        case command
        case cwd
    }
}
