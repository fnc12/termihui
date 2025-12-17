import Cocoa

/// Структура для хранения стиля текста
struct TextStyle {
    var foregroundColor: NSColor = .lightGray
    var backgroundColor: NSColor = .clear
    var isBold: Bool = false
    var isItalic: Bool = false
    var isUnderlined: Bool = false
    var isDim: Bool = false
    var isReversed: Bool = false
    
    func reset() -> TextStyle {
        return TextStyle()
    }
}

/// Сегмент текста с определённым стилем
struct StyledTextSegment {
    let text: String
    let style: TextStyle
}

/// Парсер ANSI escape-кодов
class ANSIParser {
    private var currentStyle = TextStyle()
    
    /// Парсит текст с ANSI-кодами и возвращает массив стилизованных сегментов
    func parse(_ text: String) -> [StyledTextSegment] {
        var segments: [StyledTextSegment] = []
        var currentText = ""
        var i = text.startIndex
        
        // Debug: count escape sequences
        var escCount = 0
        var oscCount = 0
        var csiCount = 0
        
        while i < text.endIndex {
            let char = text[i]
            
            // Проверяем на начало escape-последовательности
            var isEscapeStart = false
            var sequenceType = ""
            
            // 7-bit: ESC followed by [ or ]
            if char == "\u{1B}" && i < text.index(before: text.endIndex) {
                let nextChar = text[text.index(after: i)]
                if nextChar == "[" {
                    isEscapeStart = true
                    sequenceType = "CSI"
                    csiCount += 1
                } else if nextChar == "]" {
                    isEscapeStart = true
                    sequenceType = "OSC"
                    oscCount += 1
                } else if nextChar == "(" || nextChar == ")" ||
                          nextChar == "=" || nextChar == ">" || nextChar == "M" || nextChar == "D" ||
                          nextChar == "E" || nextChar == "7" || nextChar == "8" {
                    isEscapeStart = true
                    sequenceType = "ESC\(nextChar)"
                    escCount += 1
                }
            }
            
            // 8-bit: CSI (\x9B) или OSC (\x9D)
            if char == "\u{9B}" {
                isEscapeStart = true
                sequenceType = "8bit-CSI"
                csiCount += 1
            } else if char == "\u{9D}" {
                isEscapeStart = true
                sequenceType = "8bit-OSC"
                oscCount += 1
            }
            
            if isEscapeStart {
                // Добавляем накопленный текст с текущим стилем
                if !currentText.isEmpty {
                    segments.append(StyledTextSegment(text: currentText, style: currentStyle))
                    currentText = ""
                }
                
                // Парсим escape-код
                let (newIndex, parsedStyle) = parseEscapeSequence(text, from: i)
                currentStyle = parsedStyle
                i = newIndex
            } else {
                // Обычный символ
                currentText.append(char)
                i = text.index(after: i)
            }
        }
        
        // Добавляем оставшийся текст
        if !currentText.isEmpty {
            segments.append(StyledTextSegment(text: currentText, style: currentStyle))
        }
        
        // Debug output
        if csiCount > 0 || oscCount > 0 || escCount > 0 {
            print("🎨 ANSI Parser: CSI=\(csiCount), OSC=\(oscCount), ESC=\(escCount), segments=\(segments.count)")
        }
        
        // Debug: show first 100 bytes as hex if there are unparsed escape-like sequences
        let resultText = segments.map { $0.text }.joined()
        if resultText.contains("[") && resultText.contains("m") {
            let preview = String(text.prefix(100))
            let hexBytes = preview.unicodeScalars.map { String(format: "%02X", $0.value) }.joined(separator: " ")
            print("⚠️ Possible unparsed escapes. First 100 bytes: \(hexBytes)")
        }
        
        return segments
    }
    
    private func parseEscapeSequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        let char = text[startIndex]
        
        // 8-bit CSI (\x9B) - эквивалент ESC[
        if char == "\u{9B}" {
            return parseCSISequence8bit(text, from: startIndex)
        }
        
        // 8-bit OSC (\x9D) - эквивалент ESC]
        if char == "\u{9D}" {
            return parseOSCSequence8bit(text, from: startIndex)
        }
        
        // Проверяем тип 7-bit escape-последовательности
        if startIndex < text.index(before: text.endIndex) {
            let nextChar = text[text.index(after: startIndex)]
            
            if nextChar == "[" {
                // CSI последовательность (Control Sequence Introducer)
                return parseCSISequence(text, from: startIndex)
            } else if nextChar == "]" {
                // OSC последовательность (Operating System Command) - заголовки окна
                return parseOSCSequence(text, from: startIndex)
            } else if nextChar == "(" || nextChar == ")" {
                // Charset selection (ESC(B, ESC)0, etc.) - пропускаем 3 символа
                let endIndex = text.index(startIndex, offsetBy: 3, limitedBy: text.endIndex) ?? text.endIndex
                return (endIndex, currentStyle)
            } else if nextChar == "=" || nextChar == ">" {
                // Application/Normal keypad mode - пропускаем 2 символа
                return (text.index(startIndex, offsetBy: 2), currentStyle)
            } else if nextChar == "M" || nextChar == "D" || nextChar == "E" {
                // Cursor movement (reverse index, index, next line) - пропускаем 2 символа
                return (text.index(startIndex, offsetBy: 2), currentStyle)
            } else if nextChar == "7" || nextChar == "8" {
                // Save/restore cursor - пропускаем 2 символа
                return (text.index(startIndex, offsetBy: 2), currentStyle)
            }
        }
        
        // Пропускаем неизвестные escape-последовательности (только ESC символ)
        return (text.index(after: startIndex), currentStyle)
    }
    
    /// Парсит 8-bit CSI (\x9B...) последовательность
    private func parseCSISequence8bit(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        var i = text.index(after: startIndex) // Пропускаем \x9B
        var code = ""
        var isPrivate = false
        
        // Проверяем на приватный режим (?)
        if i < text.endIndex && text[i] == "?" {
            isPrivate = true
            i = text.index(after: i)
        }
        
        // Читаем до буквы (команды)
        while i < text.endIndex {
            let char = text[i]
            if char.isLetter || char == "@" || char == "`" || char == "~" {
                i = text.index(after: i)
                if isPrivate {
                    return (i, currentStyle)
                }
                let newStyle = processANSICode(code + String(char))
                return (i, newStyle)
            } else {
                code.append(char)
                i = text.index(after: i)
            }
        }
        
        return (i, currentStyle)
    }
    
    /// Парсит 8-bit OSC (\x9D...) последовательность
    private func parseOSCSequence8bit(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        var i = text.index(after: startIndex) // Пропускаем \x9D
        
        while i < text.endIndex {
            let char = text[i]
            
            if char == "\u{07}" || char == "\u{9C}" { // BEL или 8-bit ST
                return (text.index(after: i), currentStyle)
            } else if char == "\u{1B}" {
                if i < text.index(before: text.endIndex) && text[text.index(after: i)] == "\\" {
                    return (text.index(i, offsetBy: 2), currentStyle)
                } else {
                    return (i, currentStyle)
                }
            } else if char == "\u{9B}" || char == "\u{9D}" {
                // Начало новой 8-bit последовательности
                return (i, currentStyle)
            }
            
            i = text.index(after: i)
        }
        
        return (i, currentStyle)
    }
    
    private func parseCSISequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        var i = text.index(startIndex, offsetBy: 2) // Пропускаем "\x1B["
        var code = ""
        var isPrivate = false
        
        // Проверяем на приватный режим (?)
        if i < text.endIndex && text[i] == "?" {
            isPrivate = true
            i = text.index(after: i)
        }
        
        // Читаем до буквы (команды) или другого терминатора
        while i < text.endIndex {
            let char = text[i]
            // Команда CSI заканчивается буквой (a-zA-Z) или @ ` ~
            if char.isLetter || char == "@" || char == "`" || char == "~" {
                i = text.index(after: i)
                
                // Приватные режимы (h/l с ?) просто игнорируем
                if isPrivate {
                    return (i, currentStyle)
                }
                
                // Обрабатываем стандартные команды
                let newStyle = processANSICode(code + String(char))
                return (i, newStyle)
            } else {
                code.append(char)
                i = text.index(after: i)
            }
        }
        
        return (i, currentStyle)
    }
    
    private func parseOSCSequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        // OSC последовательности заканчиваются на:
        // - \x07 (BEL)
        // - \x1B\\ (ST - String Terminator)
        // - \x9C (8-bit ST)
        // - или на следующей ESC последовательности
        var i = text.index(startIndex, offsetBy: 2) // Пропускаем "\x1B]"
        
        while i < text.endIndex {
            let char = text[i]
            
            if char == "\u{07}" { // BEL
                return (text.index(after: i), currentStyle)
            } else if char == "\u{9C}" { // 8-bit ST
                return (text.index(after: i), currentStyle)
            } else if char == "\u{1B}" {
                // Проверяем ST (\x1B\\) или начало новой последовательности
                if i < text.index(before: text.endIndex) {
                    let nextChar = text[text.index(after: i)]
                    if nextChar == "\\" { // ST (String Terminator)
                        return (text.index(i, offsetBy: 2), currentStyle)
                    } else {
                        // Начало новой ESC последовательности - заканчиваем OSC тут
                        return (i, currentStyle)
                    }
                }
            }
            
            i = text.index(after: i)
        }
        
        return (i, currentStyle)
    }
    
    private func processANSICode(_ code: String) -> TextStyle {
        var newStyle = currentStyle
        
        // Убираем последний символ (команду) и парсим параметры
        let command = code.last!
        let parameters = String(code.dropLast())
        
        switch command {
        case "m": // SGR (Select Graphic Rendition) - стили и цвета
            let codes = parameters.split(separator: ";").compactMap { Int($0) }
            if codes.isEmpty {
                newStyle = newStyle.reset() // \x1B[m эквивалентно \x1B[0m
            } else {
                for code in codes {
                    newStyle = applySGRCode(code, to: newStyle)
                }
            }
        // Cursor positioning - игнорируем
        case "H", "f": break // Set cursor position
        case "A", "B", "C", "D": break // Cursor up/down/forward/back
        case "E", "F": break // Cursor next/previous line
        case "G": break // Cursor horizontal absolute
        case "d": break // Cursor vertical absolute
        case "s", "u": break // Save/restore cursor position
        
        // Editing - игнорируем
        case "J", "K": break // Erase display/line
        case "L", "M": break // Insert/delete lines
        case "P", "X", "@": break // Delete/erase/insert characters
        case "S", "T": break // Scroll up/down
        
        // Other - игнорируем
        case "n": break // Device status report
        case "r": break // Set scrolling region
        case "h", "l": break // Set/reset mode
        case "c": break // Device attributes
        case "q": break // Set cursor style
        case "t": break // Window manipulation
        
        default:
            break
        }
        
        return newStyle
    }
    
    private func applySGRCode(_ code: Int, to style: TextStyle) -> TextStyle {
        var newStyle = style
        
        switch code {
        case 0:  // Сброс
            newStyle = newStyle.reset()
        case 1:  // Жирный
            newStyle.isBold = true
        case 2:  // Тусклый
            newStyle.isDim = true
        case 3:  // Курсив
            newStyle.isItalic = true
        case 4:  // Подчёркивание
            newStyle.isUnderlined = true
        case 7:  // Инверсия
            newStyle.isReversed = true
        case 22: // Не жирный, не тусклый
            newStyle.isBold = false
            newStyle.isDim = false
        case 23: // Не курсив
            newStyle.isItalic = false
        case 24: // Не подчёркнутый
            newStyle.isUnderlined = false
        case 27: // Не инвертированный
            newStyle.isReversed = false
        
        // Цвета текста (стандартные)
        case 30: newStyle.foregroundColor = .black
        case 31: newStyle.foregroundColor = .red
        case 32: newStyle.foregroundColor = .green
        case 33: newStyle.foregroundColor = .yellow
        case 34: newStyle.foregroundColor = .blue
        case 35: newStyle.foregroundColor = .magenta
        case 36: newStyle.foregroundColor = .cyan
        case 37: newStyle.foregroundColor = .white
        case 39: newStyle.foregroundColor = .lightGray // Сброс к дефолтному
        
        // Яркие цвета текста
        case 90: newStyle.foregroundColor = NSColor.darkGray
        case 91: newStyle.foregroundColor = NSColor.systemRed
        case 92: newStyle.foregroundColor = NSColor.systemGreen
        case 93: newStyle.foregroundColor = NSColor.systemYellow
        case 94: newStyle.foregroundColor = NSColor.systemBlue
        case 95: newStyle.foregroundColor = NSColor.systemPurple
        case 96: newStyle.foregroundColor = NSColor.systemTeal
        case 97: newStyle.foregroundColor = NSColor.white
        
        // Цвета фона
        case 40: newStyle.backgroundColor = .black
        case 41: newStyle.backgroundColor = .red
        case 42: newStyle.backgroundColor = .green
        case 43: newStyle.backgroundColor = .yellow
        case 44: newStyle.backgroundColor = .blue
        case 45: newStyle.backgroundColor = .magenta
        case 46: newStyle.backgroundColor = .cyan
        case 47: newStyle.backgroundColor = .white
        case 49: newStyle.backgroundColor = .clear // Сброс к дефолтному
        
        default:
            break
        }
        
        return newStyle
    }
}

/// Расширение для конвертации в NSAttributedString
extension Array where Element == StyledTextSegment {
    func toAttributedString() -> NSAttributedString {
        let result = NSMutableAttributedString()
        
        for segment in self {
            let attributes = segment.style.toAttributes()
            let attributedText = NSAttributedString(string: segment.text, attributes: attributes)
            result.append(attributedText)
        }
        
        return result
    }
}

extension TextStyle {
    func toAttributes() -> [NSAttributedString.Key: Any] {
        var attributes: [NSAttributedString.Key: Any] = [:]
        
        // Цвет текста
        let textColor = isReversed ? backgroundColor : foregroundColor
        attributes[.foregroundColor] = textColor
        
        // Цвет фона
        let bgColor = isReversed ? foregroundColor : backgroundColor
        attributes[.backgroundColor] = bgColor
        
        // Шрифт
        var font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        
        if isBold {
            font = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        }
        
        // Курсив (сложнее с моноширинными шрифтами)
        if isItalic {
            font = NSFontManager.shared.convert(font, toHaveTrait: .italicFontMask)
        }
        
        attributes[.font] = font
        
        // Подчёркивание
        if isUnderlined {
            attributes[.underlineStyle] = NSUnderlineStyle.single.rawValue
        }
        
        return attributes
    }
}