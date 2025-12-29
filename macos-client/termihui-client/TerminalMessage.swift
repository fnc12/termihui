import Foundation

/// Command history record received from server
struct CommandHistoryRecord: Codable {
    let command: String
    let segments: [StyledSegment]
    let exitCode: Int
    let cwdStart: String
    let cwdEnd: String
    let isFinished: Bool
    
    private enum CodingKeys: String, CodingKey {
        case command, segments
        case exitCode = "exit_code"
        case cwdStart = "cwd_start"
        case cwdEnd = "cwd_end"
        case isFinished = "is_finished"
    }
}
