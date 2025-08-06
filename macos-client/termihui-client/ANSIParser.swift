import Cocoa

/// Структура для хранения стиля текста
struct TextStyle {
    var foregroundColor: NSColor = .green
    var backgroundColor: NSColor = .black
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
        
        while i < text.endIndex {
            if text[i] == "\u{1B}" && i < text.index(before: text.endIndex) && text[text.index(after: i)] == "[" {
                // Найден ANSI escape-код
                
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
                currentText.append(text[i])
                i = text.index(after: i)
            }
        }
        
        // Добавляем оставшийся текст
        if !currentText.isEmpty {
            segments.append(StyledTextSegment(text: currentText, style: currentStyle))
        }
        
        return segments
    }
    
    private func parseEscapeSequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        // Проверяем тип escape-последовательности
        if startIndex < text.index(before: text.endIndex) {
            let nextChar = text[text.index(after: startIndex)]
            
            if nextChar == "[" {
                // CSI последовательность (Control Sequence Introducer)
                return parseCSISequence(text, from: startIndex)
            } else if nextChar == "]" {
                // OSC последовательность (Operating System Command) - заголовки окна
                return parseOSCSequence(text, from: startIndex)
            }
        }
        
        // Пропускаем неизвестные escape-последовательности
        return (text.index(after: startIndex), currentStyle)
    }
    
    private func parseCSISequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        var i = text.index(startIndex, offsetBy: 2) // Пропускаем "\x1B["
        var code = ""
        
        // Читаем до буквы (команды)
        while i < text.endIndex {
            let char = text[i]
            if char.isLetter {
                // Найдена команда, обрабатываем
                let newStyle = processANSICode(code + String(char))
                return (text.index(after: i), newStyle)
            } else {
                code.append(char)
                i = text.index(after: i)
            }
        }
        
        return (i, currentStyle)
    }
    
    private func parseOSCSequence(_ text: String, from startIndex: String.Index) -> (String.Index, TextStyle) {
        // OSC последовательности заканчиваются на \x07 (BEL) или \x1B\\ (ST)
        var i = text.index(startIndex, offsetBy: 2) // Пропускаем "\x1B]"
        
        while i < text.endIndex {
            let char = text[i]
            
            if char == "\u{07}" { // BEL
                return (text.index(after: i), currentStyle)
            } else if char == "\u{1B}" && i < text.index(before: text.endIndex) {
                let nextChar = text[text.index(after: i)]
                if nextChar == "\\" { // ST (String Terminator)
                    return (text.index(i, offsetBy: 2), currentStyle)
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
        case "H", "f": // Позиционирование курсора (пока игнорируем)
            break
        case "J", "K": // Очистка (пока игнорируем)
            break
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
        case 39: newStyle.foregroundColor = .green // Сброс к дефолтному
        
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
        case 49: newStyle.backgroundColor = .black // Сброс к дефолтному
        
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