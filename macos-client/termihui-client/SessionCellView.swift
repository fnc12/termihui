import Cocoa
import SnapKit

/// Table cell view for displaying a session in the list
class SessionCellView: NSTableCellView {
    
    private let sessionLabel = NSTextField(labelWithString: "")
    private let deleteButton = NSButton()
    
    var onDelete: (() -> Void)?
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        setupUI()
    }
    
    private func setupUI() {
        // Session label
        sessionLabel.font = NSFont.systemFont(ofSize: 13)
        sessionLabel.lineBreakMode = .byTruncatingTail
        addSubview(sessionLabel)
        
        // Delete button
        deleteButton.bezelStyle = .inline
        deleteButton.isBordered = false
        deleteButton.title = ""
        if let image = NSImage(systemSymbolName: "xmark.circle", accessibilityDescription: "Delete") {
            deleteButton.image = image
            deleteButton.imagePosition = .imageOnly
        }
        deleteButton.target = self
        deleteButton.action = #selector(deleteClicked)
        deleteButton.isHidden = true // Show on hover
        addSubview(deleteButton)
        
        sessionLabel.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(4)
            make.centerY.equalToSuperview()
            make.trailing.equalTo(deleteButton.snp.leading).offset(-4)
        }
        
        deleteButton.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-4)
            make.centerY.equalToSuperview()
            make.width.height.equalTo(16)
        }
        
        // Track mouse for hover effect
        let trackingArea = NSTrackingArea(
            rect: bounds,
            options: [.mouseEnteredAndExited, .activeInKeyWindow, .inVisibleRect],
            owner: self,
            userInfo: nil
        )
        addTrackingArea(trackingArea)
    }
    
    func configure(sessionId: UInt64, isActive: Bool) {
        sessionLabel.stringValue = "#\(sessionId)"
        sessionLabel.textColor = isActive ? .controlAccentColor : .labelColor
        sessionLabel.font = isActive 
            ? NSFont.boldSystemFont(ofSize: 13) 
            : NSFont.systemFont(ofSize: 13)
    }
    
    @objc private func deleteClicked() {
        onDelete?()
    }
    
    override func mouseEntered(with event: NSEvent) {
        deleteButton.isHidden = false
    }
    
    override func mouseExited(with event: NSEvent) {
        deleteButton.isHidden = true
    }
}

