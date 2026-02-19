import Foundation
import UIKit

/// Color from C++ ANSI parser
struct SegmentColorShared: Codable {
    var name: String?
    var index: Int?
    var rgb: String?
    
    init(from decoder: Decoder) throws {
        if let str = try? decoder.singleValueContainer().decode(String.self) {
            self.name = str
        } else {
            let container = try decoder.container(keyedBy: CodingKeys.self)
            self.index = try container.decodeIfPresent(Int.self, forKey: .index)
            self.rgb = try container.decodeIfPresent(String.self, forKey: .rgb)
        }
    }
    
    func encode(to encoder: Encoder) throws {
        if let name = name {
            var container = encoder.singleValueContainer()
            try container.encode(name)
        } else {
            var container = encoder.container(keyedBy: CodingKeys.self)
            try container.encodeIfPresent(index, forKey: .index)
            try container.encodeIfPresent(rgb, forKey: .rgb)
        }
    }
    
    enum CodingKeys: String, CodingKey {
        case index, rgb
    }
    
    /// Convert to UIColor
    func toPlatformColor() -> UIColor {
        // Standard color names
        if let name = name {
            switch name {
            case "black": return .black
            case "red": return .systemRed
            case "green": return .systemGreen
            case "yellow": return .systemYellow
            case "blue": return .systemBlue
            case "magenta": return .magenta
            case "cyan": return .cyan
            case "white": return .white
            case "bright_black": return .darkGray
            case "bright_red": return .systemRed
            case "bright_green": return .systemGreen
            case "bright_yellow": return .systemYellow
            case "bright_blue": return .systemBlue
            case "bright_magenta": return .magenta
            case "bright_cyan": return .cyan
            case "bright_white": return .white
            default: return .lightGray
            }
        }
        
        // RGB hex color
        if let rgb = rgb, rgb.hasPrefix("#") {
            let hex = String(rgb.dropFirst())
            guard let value = UInt64(hex, radix: 16), hex.count == 6 else {
                return .lightGray
            }
            let r = CGFloat((value >> 16) & 0xFF) / 255.0
            let g = CGFloat((value >> 8) & 0xFF) / 255.0
            let b = CGFloat(value & 0xFF) / 255.0
            return UIColor(red: r, green: g, blue: b, alpha: 1.0)
        }
        
        // 256-color indexed
        if let index = index {
            return color256(index)
        }
        
        return .lightGray
    }
    
    private func color256(_ index: Int) -> UIColor {
        if index < 8 {
            let colors: [UIColor] = [.black, .systemRed, .systemGreen, .systemYellow, .systemBlue, .magenta, .cyan, .white]
            return colors[index]
        } else if index < 16 {
            let colors: [UIColor] = [.darkGray, .systemRed, .systemGreen, .systemYellow, .systemBlue, .magenta, .cyan, .white]
            return colors[index - 8]
        } else if index < 232 {
            let i = index - 16
            let r = CGFloat((i / 36) % 6) / 5.0
            let g = CGFloat((i / 6) % 6) / 5.0
            let b = CGFloat(i % 6) / 5.0
            return UIColor(red: r, green: g, blue: b, alpha: 1.0)
        } else {
            let gray = CGFloat(index - 232) / 23.0
            return UIColor(white: gray, alpha: 1.0)
        }
    }
}

/// Text style from C++ ANSI parser
struct SegmentStyleShared: Codable {
    var fg: SegmentColorShared?
    var bg: SegmentColorShared?
    var bold: Bool = false
    var dim: Bool = false
    var italic: Bool = false
    var underline: Bool = false
    var reverse: Bool = false
    var strikethrough: Bool = false
}

/// Styled text segment from C++ ANSI parser
struct StyledSegmentShared: Codable {
    var text: String
    var style: SegmentStyleShared
}

/// Screen row update for block mode / interactive mode diff
struct ScreenRowUpdateShared: Codable {
    let row: Int
    let segments: [StyledSegmentShared]
}

/// Command history record received from server
struct CommandHistoryRecordShared: Codable {
    let id: UInt64
    let command: String
    let segments: [StyledSegmentShared]
    let exitCode: Int
    let cwdStart: String
    let cwdEnd: String
    let isFinished: Bool
    
    private enum CodingKeys: String, CodingKey {
        case id, command, segments
        case exitCode = "exit_code"
        case cwdStart = "cwd_start"
        case cwdEnd = "cwd_end"
        case isFinished = "is_finished"
    }
}
