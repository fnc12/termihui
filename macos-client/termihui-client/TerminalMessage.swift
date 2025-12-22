import Foundation

/// Command history record received from server
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
