import UIKit
import SnapKit

/// Table view cell for chat messages
class ChatMessageCell: UITableViewCell {
    
    static let reuseIdentifier = "ChatMessageCell"
    
    // MARK: - UI Components
    private let bubbleView = UIView()
    private let messageLabel = UILabel()
    private let roleIndicator = UIView()
    
    // MARK: - Constants
    private let horizontalPadding: CGFloat = 16
    private let bubblePadding: CGFloat = 12
    private let maxBubbleWidthRatio: CGFloat = 0.8
    
    // MARK: - Init
    
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: style, reuseIdentifier: reuseIdentifier)
        setupUI()
    }
    
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        selectionStyle = .none
        backgroundColor = .clear
        contentView.backgroundColor = .clear
        
        // Bubble view
        bubbleView.layer.cornerRadius = 16
        bubbleView.clipsToBounds = true
        contentView.addSubview(bubbleView)
        
        // Role indicator (small dot)
        roleIndicator.layer.cornerRadius = 4
        contentView.addSubview(roleIndicator)
        
        // Message label
        messageLabel.numberOfLines = 0
        messageLabel.font = .systemFont(ofSize: 16)
        bubbleView.addSubview(messageLabel)
        
        messageLabel.snp.makeConstraints { make in
            make.edges.equalToSuperview().inset(bubblePadding)
        }
    }
    
    // MARK: - Configuration
    
    /// Update only text content (for streaming updates without full reconfigure)
    func updateText(_ text: String) {
        messageLabel.text = text
    }
    
    func configure(with message: ChatMessage) {
        messageLabel.text = message.content
        
        // Reset constraints before reconfiguring
        bubbleView.snp.removeConstraints()
        roleIndicator.snp.removeConstraints()
        
        switch message.role {
        case .user:
            configureAsUser()
        case .assistant:
            configureAsAssistant(isStreaming: message.isStreaming)
        case .error:
            configureAsError()
        }
    }
    
    private func configureAsUser() {
        // User messages: right-aligned, blue bubble
        bubbleView.backgroundColor = .systemBlue
        messageLabel.textColor = .white
        roleIndicator.backgroundColor = .systemBlue
        roleIndicator.isHidden = false
        
        bubbleView.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(4)
            make.bottom.equalToSuperview().offset(-4)
            make.trailing.equalToSuperview().offset(-horizontalPadding)
            make.width.lessThanOrEqualToSuperview().multipliedBy(maxBubbleWidthRatio)
        }
        
        roleIndicator.snp.makeConstraints { make in
            make.width.height.equalTo(8)
            make.trailing.equalTo(bubbleView.snp.leading).offset(-6)
            make.top.equalTo(bubbleView).offset(bubblePadding)
        }
    }
    
    private func configureAsAssistant(isStreaming: Bool) {
        // Assistant messages: left-aligned, gray bubble
        bubbleView.backgroundColor = .secondarySystemBackground
        messageLabel.textColor = .label
        roleIndicator.backgroundColor = .systemGreen
        roleIndicator.isHidden = false
        
        // Streaming indicator (pulsing alpha)
        if isStreaming {
            startStreamingAnimation()
        } else {
            stopStreamingAnimation()
        }
        
        bubbleView.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(4)
            make.bottom.equalToSuperview().offset(-4)
            make.leading.equalToSuperview().offset(horizontalPadding)
            make.width.lessThanOrEqualToSuperview().multipliedBy(maxBubbleWidthRatio)
        }
        
        roleIndicator.snp.makeConstraints { make in
            make.width.height.equalTo(8)
            make.leading.equalTo(bubbleView.snp.trailing).offset(6)
            make.top.equalTo(bubbleView).offset(bubblePadding)
        }
    }
    
    private func configureAsError() {
        // Error messages: centered, red bubble
        bubbleView.backgroundColor = UIColor.systemRed.withAlphaComponent(0.15)
        messageLabel.textColor = .systemRed
        roleIndicator.isHidden = true
        
        bubbleView.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(4)
            make.bottom.equalToSuperview().offset(-4)
            make.centerX.equalToSuperview()
            make.width.lessThanOrEqualToSuperview().multipliedBy(0.9)
        }
    }
    
    // MARK: - Streaming Animation
    
    private func startStreamingAnimation() {
        roleIndicator.alpha = 1.0
        UIView.animate(withDuration: 0.6, delay: 0, options: [.repeat, .autoreverse, .allowUserInteraction]) {
            self.roleIndicator.alpha = 0.3
        }
    }
    
    private func stopStreamingAnimation() {
        roleIndicator.layer.removeAllAnimations()
        roleIndicator.alpha = 1.0
    }
    
    // MARK: - Reuse
    
    override func prepareForReuse() {
        super.prepareForReuse()
        stopStreamingAnimation()
        messageLabel.text = nil
        roleIndicator.isHidden = false
    }
}
