import UIKit
import SnapKit

/// Cell for displaying a single command block in terminal
class CommandBlockCell: UITableViewCell {
    
    static let reuseIdentifier = "CommandBlockCell"
    
    // MARK: - UI
    private let commandLabel = UILabel()
    private let outputTextView = UITextView()
    private let statusView = UIView()
    private let exitCodeLabel = UILabel()
    
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
        backgroundColor = .systemBackground
        selectionStyle = .none
        
        // Command label ($ command)
        commandLabel.font = UIFont.monospacedSystemFont(ofSize: 14, weight: .bold)
        commandLabel.textColor = .systemGreen
        commandLabel.numberOfLines = 1
        
        // Output text view
        outputTextView.isEditable = false
        outputTextView.isScrollEnabled = false // Important for self-sizing
        outputTextView.font = UIFont.monospacedSystemFont(ofSize: 14, weight: .regular)
        outputTextView.backgroundColor = .clear
        outputTextView.textColor = .label
        outputTextView.textContainerInset = .zero
        outputTextView.textContainer.lineFragmentPadding = 0
        
        // Status view (colored bar on the left)
        statusView.layer.cornerRadius = 2
        
        // Exit code label
        exitCodeLabel.font = UIFont.monospacedSystemFont(ofSize: 10, weight: .regular)
        exitCodeLabel.textColor = .secondaryLabel
        
        // Layout
        contentView.addSubview(statusView)
        contentView.addSubview(commandLabel)
        contentView.addSubview(outputTextView)
        contentView.addSubview(exitCodeLabel)
        
        statusView.snp.makeConstraints { make in
            make.leading.equalToSuperview().offset(8)
            make.top.equalToSuperview().offset(8)
            make.bottom.equalToSuperview().offset(-8)
            make.width.equalTo(4)
        }
        
        commandLabel.snp.makeConstraints { make in
            make.leading.equalTo(statusView.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-8)
            make.top.equalToSuperview().offset(8)
        }
        
        outputTextView.snp.makeConstraints { make in
            make.leading.equalTo(statusView.snp.trailing).offset(8)
            make.trailing.equalToSuperview().offset(-8)
            make.top.equalTo(commandLabel.snp.bottom).offset(4)
            make.bottom.equalToSuperview().offset(-8)
        }
        
        exitCodeLabel.snp.makeConstraints { make in
            make.trailing.equalToSuperview().offset(-8)
            make.top.equalToSuperview().offset(8)
        }
    }
    
    // MARK: - Configure
    
    func configure(with block: CommandBlockModel) {
        // Command
        if let command = block.command, !command.isEmpty {
            commandLabel.text = "$ \(command)"
            commandLabel.isHidden = false
        } else {
            commandLabel.isHidden = true
        }
        
        // Output
        if !block.segments.isEmpty {
            outputTextView.attributedText = attributedString(from: block.segments)
            outputTextView.isHidden = false
        } else {
            outputTextView.isHidden = true
        }
        
        // Status bar color
        if !block.isFinished {
            statusView.backgroundColor = .systemBlue // Running
            exitCodeLabel.text = "running..."
            exitCodeLabel.textColor = .systemBlue
        } else if block.exitCode == 0 {
            statusView.backgroundColor = .systemGreen // Success
            exitCodeLabel.text = nil
        } else {
            statusView.backgroundColor = .systemRed // Error
            exitCodeLabel.text = "exit \(block.exitCode)"
            exitCodeLabel.textColor = .systemRed
        }
    }
    
    // MARK: - Helpers
    
    private func attributedString(from segments: [StyledSegmentShared]) -> NSAttributedString {
        let result = NSMutableAttributedString()
        
        for segment in segments {
            var attrs: [NSAttributedString.Key: Any] = [
                .font: UIFont.monospacedSystemFont(ofSize: 14, weight: segment.style.bold ? .bold : .regular)
            ]
            
            if let fg = segment.style.fg {
                attrs[.foregroundColor] = fg.toPlatformColor()
            } else {
                attrs[.foregroundColor] = UIColor.label
            }
            
            if let bg = segment.style.bg {
                attrs[.backgroundColor] = bg.toPlatformColor()
            }
            
            if segment.style.underline {
                attrs[.underlineStyle] = NSUnderlineStyle.single.rawValue
            }
            
            if segment.style.strikethrough {
                attrs[.strikethroughStyle] = NSUnderlineStyle.single.rawValue
            }
            
            result.append(NSAttributedString(string: segment.text, attributes: attrs))
        }
        
        return result
    }
}
