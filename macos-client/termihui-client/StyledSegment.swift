import Cocoa

/// Color from C++ ANSI parser
struct SegmentColor: Codable {
    // For standard/bright colors - just a string like "red", "bright_green"
    // For indexed - {"index": 196}
    // For RGB - {"rgb": "#FF5733"}
    
    var name: String?
    var index: Int?
    var rgb: String?
    
    init(from decoder: Decoder) throws {
        // Can be either a string or an object
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
    
    /// Convert to NSColor
    func toNSColor() -> NSColor {
        // Standard color names
        if let name = name {
            switch name {
            case "black": return .black
            case "red": return .systemRed
            case "green": return .systemGreen
            case "yellow": return .systemYellow
            case "blue": return .systemBlue
            case "magenta": return .systemPurple
            case "cyan": return .cyan
            case "white": return .white
            case "bright_black": return .darkGray
            case "bright_red": return .systemRed.withAlphaComponent(1.0)
            case "bright_green": return .systemGreen.withAlphaComponent(1.0)
            case "bright_yellow": return .systemYellow.withAlphaComponent(1.0)
            case "bright_blue": return .systemBlue.withAlphaComponent(1.0)
            case "bright_magenta": return .systemPurple.withAlphaComponent(1.0)
            case "bright_cyan": return .cyan.withAlphaComponent(1.0)
            case "bright_white": return .white
            default: return .lightGray
            }
        }
        
        // RGB hex color
        if let rgb = rgb, rgb.hasPrefix("#") {
            let hex = String(rgb.dropFirst())
            if let value = UInt64(hex, radix: 16), hex.count == 6 {
                let r = CGFloat((value >> 16) & 0xFF) / 255.0
                let g = CGFloat((value >> 8) & 0xFF) / 255.0
                let b = CGFloat(value & 0xFF) / 255.0
                return NSColor(red: r, green: g, blue: b, alpha: 1.0)
            }
        }
        
        // 256-color indexed
        if let index = index {
            return color256(index)
        }
        
        return .lightGray
    }
    
    /// Convert 256-color index to NSColor
    private func color256(_ index: Int) -> NSColor {
        if index < 8 {
            // Standard colors
            let colors: [NSColor] = [.black, .systemRed, .systemGreen, .systemYellow, .systemBlue, .systemPurple, .cyan, .white]
            return colors[index]
        } else if index < 16 {
            // Bright colors
            let colors: [NSColor] = [.darkGray, .systemRed, .systemGreen, .systemYellow, .systemBlue, .systemPurple, .cyan, .white]
            return colors[index - 8]
        } else if index < 232 {
            // 216 color cube (6x6x6)
            let i = index - 16
            let r = CGFloat((i / 36) % 6) / 5.0
            let g = CGFloat((i / 6) % 6) / 5.0
            let b = CGFloat(i % 6) / 5.0
            return NSColor(red: r, green: g, blue: b, alpha: 1.0)
        } else {
            // Grayscale (24 shades)
            let gray = CGFloat(index - 232) / 23.0
            return NSColor(white: gray, alpha: 1.0)
        }
    }
}

/// Text style from C++ ANSI parser
struct SegmentStyle: Codable {
    var fg: SegmentColor?
    var bg: SegmentColor?
    var bold: Bool = false
    var dim: Bool = false
    var italic: Bool = false
    var underline: Bool = false
    var reverse: Bool = false
    var strikethrough: Bool = false
    
    /// Convert to NSAttributedString attributes
    func toAttributes() -> [NSAttributedString.Key: Any] {
        var attrs: [NSAttributedString.Key: Any] = [:]
        
        // Foreground color
        var fgColor = fg?.toNSColor() ?? .lightGray
        var bgColor = bg?.toNSColor() ?? .clear
        
        // Handle reverse (swap foreground and background)
        if reverse {
            swap(&fgColor, &bgColor)
            // If foreground became clear (was transparent background), make it black
            if fgColor == .clear {
                fgColor = .black
            }
        }
        
        attrs[.foregroundColor] = fgColor
        if bgColor != .clear {
            attrs[.backgroundColor] = bgColor
        }
        
        // Font
        var font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        if bold {
            font = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        }
        if italic {
            font = NSFontManager.shared.convert(font, toHaveTrait: .italicFontMask)
        }
        attrs[.font] = font
        
        // Underline
        if underline {
            attrs[.underlineStyle] = NSUnderlineStyle.single.rawValue
        }
        
        // Strikethrough
        if strikethrough {
            attrs[.strikethroughStyle] = NSUnderlineStyle.single.rawValue
        }
        
        return attrs
    }
}

/// Styled text segment from C++ ANSI parser
struct StyledSegment: Codable {
    var text: String
    var style: SegmentStyle
}

/// Extension to convert array of segments to NSAttributedString
extension Array where Element == StyledSegment {
    func toAttributedString() -> NSAttributedString {
        let result = NSMutableAttributedString()
        for segment in self {
            let attrs = segment.style.toAttributes()
            let attrStr = NSAttributedString(string: segment.text, attributes: attrs)
            result.append(attrStr)
        }
        return result
    }
}

