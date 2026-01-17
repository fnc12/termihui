import Foundation

/// Model for a single command block in terminal
struct CommandBlockModel {
    var id: UInt64
    var command: String?
    var segments: [StyledSegmentShared]
    var isFinished: Bool
    var exitCode: Int
    var cwdStart: String?
    var cwdEnd: String?
    
    init(id: UInt64 = 0,
         command: String? = nil,
         segments: [StyledSegmentShared] = [],
         isFinished: Bool = false,
         exitCode: Int = 0,
         cwdStart: String? = nil,
         cwdEnd: String? = nil) {
        self.id = id
        self.command = command
        self.segments = segments
        self.isFinished = isFinished
        self.exitCode = exitCode
        self.cwdStart = cwdStart
        self.cwdEnd = cwdEnd
    }
    
    /// Create from server history record
    init(from record: CommandHistoryRecordShared) {
        self.id = record.id
        self.command = record.command.isEmpty ? nil : record.command
        self.segments = record.segments
        self.isFinished = record.isFinished
        self.exitCode = record.exitCode
        self.cwdStart = record.cwdStart.isEmpty ? nil : record.cwdStart
        self.cwdEnd = record.cwdEnd.isEmpty ? nil : record.cwdEnd
    }
}
