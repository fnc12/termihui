import Cocoa

final class CommandBlockItem: NSCollectionViewItem {
    static let reuseId = NSUserInterfaceItemIdentifier("CommandBlockItem")
    
    private let headerLabel = NSTextField(labelWithString: "")
    private let bodyTextView = NSTextView()
    private let container = NSView()
    private let separatorView = NSView()
    
    override func loadView() {
        view = container
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }
    
    func configure(command: String?, output: String, isFinished: Bool, exitCode: Int?) {
        if let cmd = command, !cmd.isEmpty {
            headerLabel.stringValue = cmd
            headerLabel.isHidden = false
        } else {
            headerLabel.stringValue = ""
            headerLabel.isHidden = true
        }
        
        bodyTextView.string = output
    }
    
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.black.cgColor
        
        headerLabel.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        headerLabel.textColor = .white
        headerLabel.lineBreakMode = .byWordWrapping
        headerLabel.maximumNumberOfLines = 0
        
        bodyTextView.isEditable = false
        bodyTextView.isSelectable = true
        bodyTextView.drawsBackground = false
        bodyTextView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        bodyTextView.textColor = .white
        
        separatorView.wantsLayer = true
        separatorView.layer?.backgroundColor = NSColor.separatorColor.cgColor
        
        view.addSubview(headerLabel)
        view.addSubview(bodyTextView)
        view.addSubview(separatorView)
        
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        bodyTextView.translatesAutoresizingMaskIntoConstraints = false
        separatorView.translatesAutoresizingMaskIntoConstraints = false
        
        NSLayoutConstraint.activate([
            headerLabel.topAnchor.constraint(equalTo: view.topAnchor, constant: 8),
            headerLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 8),
            headerLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -8),
            
            bodyTextView.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 4),
            bodyTextView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 8),
            bodyTextView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -8),
            bodyTextView.bottomAnchor.constraint(equalTo: separatorView.topAnchor, constant: -8),

            separatorView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            separatorView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            separatorView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            separatorView.heightAnchor.constraint(equalToConstant: 1)
        ])
    }
    
    static func estimatedHeight(command: String?, output: String, width: CGFloat) -> CGFloat {
        let horizontalInsets: CGFloat = 16
        let constrainedWidth = max(0, width - horizontalInsets)
        
        var total: CGFloat = 16 // vertical insets
        if let cmd = command, !cmd.isEmpty {
            let rect = (cmd as NSString).boundingRect(
                with: NSSize(width: constrainedWidth, height: .greatestFiniteMagnitude),
                options: [.usesLineFragmentOrigin, .usesFontLeading],
                attributes: [.font: NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)]
            )
            total += ceil(rect.height) + 4
        }
        // Грубая оценка высоты тела
        let bodyRect = (output as NSString).boundingRect(
            with: NSSize(width: constrainedWidth, height: .greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            attributes: [.font: NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)]
        )
        total += ceil(bodyRect.height)
        return max(total, 28)
    }
}


