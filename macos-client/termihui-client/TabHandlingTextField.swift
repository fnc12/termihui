import Cocoa

/// Кастомный NSTextField с поддержкой перехвата Tab для автодополнения
class TabHandlingTextField: NSTextField {
    
    weak var tabDelegate: TabHandlingTextFieldDelegate?
    
    override func awakeFromNib() {
        super.awakeFromNib()
        setupTextField()
    }
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupTextField()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupTextField()
    }
    
    private func setupTextField() {
        // Создаем кастомный field editor для перехвата Tab
        print("🔧 TabHandlingTextField инициализирован")
    }
    
    // Переопределяем textShouldBeginEditing для установки делегата field editor
    override func textShouldBeginEditing(_ textObject: NSText) -> Bool {
        print("🔧 textShouldBeginEditing вызван")
        
        // Устанавливаем себя как делегат для field editor
        if let textView = textObject as? NSTextView {
            textView.delegate = self
            print("🔧 Делегат field editor установлен")
        }
        
        return super.textShouldBeginEditing(textObject)
    }
}

// MARK: - NSTextViewDelegate для перехвата клавиш в field editor
extension TabHandlingTextField: NSTextViewDelegate {
    
    func textView(_ textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        print("🔧 doCommandBy вызван с селектором: \(commandSelector)")
        
        // Проверяем команду Tab
        if commandSelector == #selector(NSResponder.insertTab(_:)) {
            print("🔧 Tab перехвачен через field editor!")
            print("🔧 Текущий текст: '\(self.stringValue)'")
            print("🔧 Позиция курсора: \(textView.selectedRange().location)")
            
            // Уведомляем делегата о нажатии Tab
            tabDelegate?.tabHandlingTextField(self, didPressTabWithText: self.stringValue, cursorPosition: textView.selectedRange().location)
            
            return true // Обрабатываем событие сами, не передаем дальше
        }
        
        // Для всех остальных команд - стандартное поведение
        return false
    }
}

protocol TabHandlingTextFieldDelegate: AnyObject {
    func tabHandlingTextField(_ textField: TabHandlingTextField, didPressTabWithText text: String, cursorPosition: Int)
}
