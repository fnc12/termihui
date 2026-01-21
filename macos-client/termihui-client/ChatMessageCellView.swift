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
        messageLabel.stringValue = message.content
        
        switch message.role {
        case .user:
            roleIndicator.stringValue = "You"
            bubbleView.layer?.backgroundColor = NSColor.systemBlue.withAlphaComponent(0.15).cgColor
            messageLabel.textColor = .labelColor
        case .assistant:
            roleIndicator.stringValue = "AI"
            bubbleView.layer?.backgroundColor = NSColor.systemGray.withAlphaComponent(0.15).cgColor
            messageLabel.textColor = .labelColor
        }
        
        // Streaming indicator
        if message.isStreaming && message.content.isEmpty {
            messageLabel.stringValue = "..."
        }
    }
}
