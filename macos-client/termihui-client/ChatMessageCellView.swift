import Cocoa
import SnapKit

/// Cell view for displaying chat messages
class ChatMessageCellView: NSTableCellView {
    
    private let bubbleView = NSView()
    private let messageLabel = NSTextField(wrappingLabelWithString: "")
    private let roleIndicator = NSTextField(labelWithString: "")
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupUI()
    }
    
    private func setupUI() {
        // Role indicator (You / AI)
        roleIndicator.font = NSFont.systemFont(ofSize: 10, weight: .medium)
        roleIndicator.textColor = .secondaryLabelColor
        addSubview(roleIndicator)
        
        // Bubble background
        bubbleView.wantsLayer = true
        bubbleView.layer?.cornerRadius = 8
        addSubview(bubbleView)
        
        // Message text
        messageLabel.font = NSFont.systemFont(ofSize: 13)
        messageLabel.isSelectable = true
        messageLabel.allowsEditingTextAttributes = true
        messageLabel.maximumNumberOfLines = 0
        messageLabel.lineBreakMode = .byWordWrapping
        bubbleView.addSubview(messageLabel)
        
        roleIndicator.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(4)
            make.leading.equalToSuperview().offset(8)
        }
        
        bubbleView.snp.makeConstraints { make in
            make.top.equalTo(roleIndicator.snp.bottom).offset(2)
            make.leading.equalToSuperview().offset(8)
            make.trailing.lessThanOrEqualToSuperview().offset(-8)
            make.bottom.equalToSuperview().offset(-4)
        }
        
        messageLabel.snp.makeConstraints { make in
            make.edges.equalToSuperview().inset(NSEdgeInsets(top: 6, left: 10, bottom: 6, right: 10))
            make.width.lessThanOrEqualTo(280)
        }
    }
    
    func configure(with message: ChatMessage) {
        let textColor: NSColor
        
        switch message.role {
        case .user:
            roleIndicator.stringValue = "You"
            bubbleView.layer?.backgroundColor = NSColor.systemBlue.withAlphaComponent(0.15).cgColor
            textColor = .labelColor
        case .assistant:
            roleIndicator.stringValue = "AI"
            bubbleView.layer?.backgroundColor = NSColor.systemGray.withAlphaComponent(0.15).cgColor
            textColor = .labelColor
        case .error:
            roleIndicator.stringValue = "AI"
            bubbleView.layer?.backgroundColor = NSColor.systemRed.withAlphaComponent(0.15).cgColor
            textColor = .labelColor
        }
        
        // Streaming indicator
        if message.isStreaming && message.content.isEmpty {
            messageLabel.stringValue = "..."
            messageLabel.textColor = textColor
        } else {
            messageLabel.attributedStringValue = parseMarkdown(message.content, textColor: textColor)
        }
    }
    
    /// Parse markdown string to NSAttributedString
    private func parseMarkdown(_ text: String, textColor: NSColor) -> NSAttributedString {
        // Default attributes
        let baseFont = NSFont.systemFont(ofSize: 13)
        let defaultAttributes: [NSAttributedString.Key: Any] = [
            .font: baseFont,
            .foregroundColor: textColor
        ]
        
        // Try to parse as markdown using native API (macOS 12+)
        if #available(macOS 12, *) {
            do {
                var attributedString = try AttributedString(markdown: text, options: AttributedString.MarkdownParsingOptions(
                    interpretedSyntax: .inlineOnlyPreservingWhitespace
                ))
                
                // Apply base font to the entire string
                attributedString.font = baseFont
                attributedString.foregroundColor = textColor
                
                // Convert to NSAttributedString
                let nsAttributedString = NSMutableAttributedString(attributedString)
                
                // Ensure proper font styling is applied
                nsAttributedString.enumerateAttribute(.font, in: NSRange(location: 0, length: nsAttributedString.length), options: []) { value, range, _ in
                    guard let font = value as? NSFont else { return }
                    
                    let traits = font.fontDescriptor.symbolicTraits
                    var newFont = baseFont
                    
                    if traits.contains(.bold) && traits.contains(.italic) {
                        let descriptor = baseFont.fontDescriptor.withSymbolicTraits([.bold, .italic])
                        newFont = NSFont(descriptor: descriptor, size: baseFont.pointSize) ?? baseFont
                    } else if traits.contains(.bold) {
                        newFont = NSFont.boldSystemFont(ofSize: baseFont.pointSize)
                    } else if traits.contains(.italic) {
                        newFont = NSFont.systemFont(ofSize: baseFont.pointSize).withTraits(.italic)
                    }
                    
                    // Check for monospace (code)
                    if traits.contains(.monoSpace) {
                        newFont = NSFont.monospacedSystemFont(ofSize: baseFont.pointSize - 1, weight: .regular)
                        nsAttributedString.addAttribute(.backgroundColor, value: NSColor.systemGray.withAlphaComponent(0.2), range: range)
                    }
                    
                    nsAttributedString.addAttribute(.font, value: newFont, range: range)
                }
                
                return nsAttributedString
            } catch {
                // If markdown parsing fails, return plain text
                return NSAttributedString(string: text, attributes: defaultAttributes)
            }
        } else {
            // Fallback for older macOS versions
            return NSAttributedString(string: text, attributes: defaultAttributes)
        }
    }
}

// MARK: - NSFont Extension for Traits
private extension NSFont {
    func withTraits(_ traits: NSFontDescriptor.SymbolicTraits) -> NSFont {
        let descriptor = fontDescriptor.withSymbolicTraits(traits)
        return NSFont(descriptor: descriptor, size: pointSize) ?? self
    }
}
