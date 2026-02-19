import Cocoa

final class CommandBlockItem: NSCollectionViewItem {
    static let reuseId = NSUserInterfaceItemIdentifier("CommandBlockItem")
    
    /// Horizontal padding for content (left + right)
    static let horizontalPadding: CGFloat = 8
    
    private let cwdLabel = NSTextField(labelWithString: "")
    private let headerLabel = NSTextField(labelWithString: "")
    private let bodyTextView = NSTextView()
    private let container = CommandBlockContainerView()
    private let separatorView = NSView()
    
    // Dynamic constraints for empty/non-empty output
    private var bodyBottomConstraint: NSLayoutConstraint?
    private var headerBottomConstraint: NSLayoutConstraint?
    
    // Current highlight state (for reset)
    private var lastHeaderHighlight: NSRange?
    private var hasBodyHighlight: Bool = false
    
    // Server command ID for context menu actions (nil = currently executing command)
    private var commandId: UInt64?
    
    // Delegate for context menu actions
    weak var delegate: CommandBlockItemDelegate?
    
    override func loadView() {
        view = container
        container.item = self
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupContextMenu()
    }
    
    // MARK: - Context Menu
    
    private func setupContextMenu() {
        let menu = NSMenu()
        menu.addItem(NSMenuItem(title: "Copy", action: #selector(copyAll), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Copy Command", action: #selector(copyCommand), keyEquivalent: ""))
        menu.addItem(NSMenuItem(title: "Copy Output", action: #selector(copyOutput), keyEquivalent: ""))
        container.menu = menu
    }
    
    @objc private func copyAll() {
        delegate?.commandBlockItem(self, didRequestCopyAll: commandId)
    }
    
    @objc private func copyCommand() {
        delegate?.commandBlockItem(self, didRequestCopyCommand: commandId)
    }
    
    @objc private func copyOutput() {
        delegate?.commandBlockItem(self, didRequestCopyOutput: commandId)
    }
    
    func configure(commandId: UInt64?, command: String?, outputSegments: [StyledSegment], activeScreenLines: [[StyledSegment]] = [], isFinished: Bool, exitCode: Int?, cwdStart: String?, serverHome: String = "") {
        self.commandId = commandId
        
        // CWD label — gray, above command
        if let cwd = cwdStart, !cwd.isEmpty {
            cwdLabel.stringValue = shortenHomePath(cwd, serverHome: serverHome)
            cwdLabel.isHidden = false
        } else {
            cwdLabel.stringValue = ""
            cwdLabel.isHidden = true
        }
        
        if let cmd = command, !cmd.isEmpty {
            headerLabel.stringValue = "$ " + cmd
            headerLabel.isHidden = false
        } else {
            headerLabel.stringValue = ""
            headerLabel.isHidden = true
        }
        
        // Combine committed output + active screen lines
        let hasCommittedOutput = !outputSegments.isEmpty && outputSegments.contains { !$0.text.isEmpty }
        let hasActiveLines = !activeScreenLines.isEmpty && activeScreenLines.contains { !$0.isEmpty }
        let hasOutput = hasCommittedOutput || hasActiveLines
        bodyTextView.isHidden = !hasOutput
        
        // Switch constraints based on whether we have output
        bodyBottomConstraint?.isActive = hasOutput
        headerBottomConstraint?.isActive = !hasOutput
        
        if hasOutput {
            let attributedOutput = NSMutableAttributedString()
            
            // 1. Committed output (scrolled-off lines)
            if hasCommittedOutput {
                attributedOutput.append(outputSegments.toAttributedString())
            }
            
            // 2. Active screen lines (may be overwritten by \r progress)
            if hasActiveLines {
                for (i, line) in activeScreenLines.enumerated() where !line.isEmpty {
                    if attributedOutput.length > 0 || i > 0 {
                        attributedOutput.append(NSAttributedString(string: "\n"))
                    }
                    attributedOutput.append(line.toAttributedString())
                }
            }
            
            // Apply paragraph style with tab stops to entire string
            if let paragraphStyle = bodyTextView.defaultParagraphStyle {
                attributedOutput.addAttribute(.paragraphStyle, value: paragraphStyle, range: NSRange(location: 0, length: attributedOutput.length))
            }
            
            bodyTextView.textStorage?.setAttributedString(attributedOutput)
        } else {
            bodyTextView.textStorage?.setAttributedString(NSAttributedString())
        }
        clearSelectionHighlight()
    }
    
    /// Shortens home directory to ~ using server-provided home
    private func shortenHomePath(_ path: String, serverHome: String) -> String {
        if !serverHome.isEmpty && path.hasPrefix(serverHome) {
            return "~" + String(path.dropFirst(serverHome.count))
        }
        return path
    }
    
    private func setupUI() {
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.black.cgColor
        
        // CWD label — gray, small
        cwdLabel.font = NSFont.monospacedSystemFont(ofSize: 10, weight: .regular)
        cwdLabel.textColor = NSColor.gray
        cwdLabel.lineBreakMode = .byTruncatingHead
        cwdLabel.maximumNumberOfLines = 1
        
        headerLabel.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)
        headerLabel.textColor = .white
        headerLabel.lineBreakMode = .byWordWrapping
        headerLabel.maximumNumberOfLines = 0
        
        bodyTextView.isEditable = false
        // Disable native selection so unified selection goes through controller
        bodyTextView.isSelectable = false
        bodyTextView.drawsBackground = false
        bodyTextView.font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        bodyTextView.textColor = .white
        bodyTextView.textContainerInset = NSSize(width: 0, height: 0)
        
        // Configure tab stops for proper column alignment
        let font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
        let charWidth = "M".size(withAttributes: [.font: font]).width
        let tabWidth = charWidth * 8 // 8-character tab stops
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.tabStops = (1...20).map { NSTextTab(type: .leftTabStopType, location: CGFloat($0) * tabWidth) }
        paragraphStyle.defaultTabInterval = tabWidth
        bodyTextView.defaultParagraphStyle = paragraphStyle
        
        separatorView.wantsLayer = true
        separatorView.layer?.backgroundColor = NSColor.separatorColor.cgColor
        
        view.addSubview(cwdLabel)
        view.addSubview(headerLabel)
        view.addSubview(bodyTextView)
        view.addSubview(separatorView)
        
        cwdLabel.translatesAutoresizingMaskIntoConstraints = false
        headerLabel.translatesAutoresizingMaskIntoConstraints = false
        bodyTextView.translatesAutoresizingMaskIntoConstraints = false
        separatorView.translatesAutoresizingMaskIntoConstraints = false
        
        let padding = Self.horizontalPadding
        
        // Create dynamic constraints
        bodyBottomConstraint = bodyTextView.bottomAnchor.constraint(equalTo: separatorView.topAnchor, constant: -padding)
        headerBottomConstraint = headerLabel.bottomAnchor.constraint(equalTo: separatorView.topAnchor, constant: -6)
        
        NSLayoutConstraint.activate([
            // CWD at top
            cwdLabel.topAnchor.constraint(equalTo: view.topAnchor, constant: 6),
            cwdLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: padding),
            cwdLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -padding),
            
            // Command below cwd
            headerLabel.topAnchor.constraint(equalTo: cwdLabel.bottomAnchor, constant: 2),
            headerLabel.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: padding),
            headerLabel.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -padding),
            
            bodyTextView.topAnchor.constraint(equalTo: headerLabel.bottomAnchor, constant: 4),
            bodyTextView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: padding),
            bodyTextView.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -padding),

            separatorView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            separatorView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            separatorView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            separatorView.heightAnchor.constraint(equalToConstant: 1)
        ])
        
        // Initially activate body constraint (will be toggled in configure)
        bodyBottomConstraint?.isActive = true
    }
    
    static func estimatedHeight(command: String?, outputText: String, width: CGFloat, cwdStart: String? = nil) -> CGFloat {
        let horizontalInsets: CGFloat = 16
        let constrainedWidth = max(0, width - horizontalInsets)
        
        var total: CGFloat = 6 // top padding
        
        // CWD label
        if let cwd = cwdStart, !cwd.isEmpty {
            total += 14 // cwd label height + spacing
        }
        
        if let cmd = command, !cmd.isEmpty {
            let displayCmd = "$ " + cmd
            let rect = (displayCmd as NSString).boundingRect(
                with: NSSize(width: constrainedWidth, height: .greatestFiniteMagnitude),
                options: [.usesLineFragmentOrigin, .usesFontLeading],
                attributes: [.font: NSFont.monospacedSystemFont(ofSize: 12, weight: .bold)]
            )
            total += ceil(rect.height) + 4
        }
        
        // Only add body height if there's actual output
        if !outputText.isEmpty {
            // Create paragraph style with tab stops for proper width calculation
            let font = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
            let charWidth = "M".size(withAttributes: [.font: font]).width
            let tabWidth = charWidth * 8
            let paragraphStyle = NSMutableParagraphStyle()
            paragraphStyle.tabStops = (1...20).map { NSTextTab(type: .leftTabStopType, location: CGFloat($0) * tabWidth) }
            paragraphStyle.defaultTabInterval = tabWidth
            
            let bodyRect = (outputText as NSString).boundingRect(
                with: NSSize(width: constrainedWidth, height: .greatestFiniteMagnitude),
                options: [.usesLineFragmentOrigin, .usesFontLeading],
                attributes: [.font: font, .paragraphStyle: paragraphStyle]
            )
            total += ceil(bodyRect.height) + 8 // body + bottom padding
        }
        
        total += 1 // separator
        return max(total, 28)
    }

    // MARK: - Selection Highlight API
    func clearSelectionHighlight() {
        // Reset header
        if let last = lastHeaderHighlight, last.length > 0 {
            let attr = NSMutableAttributedString(string: headerLabel.stringValue)
            headerLabel.attributedStringValue = attr
            lastHeaderHighlight = nil
        }
        // Reset body
        if hasBodyHighlight, let storage = bodyTextView.textStorage {
            storage.removeAttribute(.backgroundColor, range: NSRange(location: 0, length: storage.length))
            hasBodyHighlight = false
        }
    }

    func setSelectionHighlight(headerRange: NSRange?, bodyRange: NSRange?) {
        clearSelectionHighlight()
        if let hr = headerRange, hr.length > 0, !headerLabel.isHidden {
            let cmdText = headerLabel.stringValue // now contains "$ " + cmd
            // Full header in global document looks like "$ <cmd>\n"
            // Content part of header (without trailing "\n") starts at 0 (we already have "$ ")
            let contentStart = 0
            let contentLength = cmdText.count // without \n
            let contentInFull = NSRange(location: contentStart, length: contentLength)
            let inter = NSIntersectionRange(hr, contentInFull)
            if inter.length > 0 {
                // Direct mapping to label coordinates
                let mapped = inter
                let attr = NSMutableAttributedString(string: cmdText)
                attr.addAttribute(.backgroundColor, value: NSColor.selectedTextBackgroundColor, range: mapped)
                headerLabel.attributedStringValue = attr
                lastHeaderHighlight = mapped
            } else {
                headerLabel.attributedStringValue = NSAttributedString(string: cmdText)
                lastHeaderHighlight = nil
            }
        }
        if let br = bodyRange, br.length > 0, let storage = bodyTextView.textStorage {
            // Clamp range to UTF-16 text length
            let utf16Len = (storage.string as NSString).length
            let safeLocation = max(0, min(br.location, utf16Len))
            var safeLength = max(0, min(br.length, utf16Len - safeLocation))
            // If selection ends exactly at line boundary, extend to include newline (\n or \r\n)
            if safeLocation + safeLength < utf16Len {
                let nsString = storage.string as NSString
                let end = safeLocation + safeLength
                let ch = nsString.character(at: end)
                if ch == 10 { // \n
                    safeLength += 1
                } else if ch == 13 { // \r
                    safeLength += 1
                    if end + 1 < utf16Len, nsString.character(at: end + 1) == 10 { // \r\n
                        safeLength += 1
                    }
                }
            }
            if safeLength > 0 {
                let clamped = NSRange(location: safeLocation, length: safeLength)
                storage.addAttribute(.backgroundColor, value: NSColor.selectedTextBackgroundColor, range: clamped)
                hasBodyHighlight = true
                bodyTextView.setNeedsDisplay(bodyTextView.bounds)
            }
        }
    }

    // MARK: - Hit Testing to character index
    /// Returns character index in bodyTextView for point in item.view coordinates
    func bodyCharacterIndex(at pointInItem: NSPoint) -> Int? {
        let ptInBody = bodyTextView.convert(pointInItem, from: self.view)
        if !bodyTextView.bounds.contains(ptInBody) { return nil }
        guard let lm = bodyTextView.layoutManager, let tc = bodyTextView.textContainer else { return nil }
        let glyphPoint = NSPoint(x: ptInBody.x - bodyTextView.textContainerOrigin.x, y: ptInBody.y - bodyTextView.textContainerOrigin.y)
        var fraction: CGFloat = 0
        let idx = lm.characterIndex(for: glyphPoint, in: tc, fractionOfDistanceBetweenInsertionPoints: &fraction)
        return idx
    }

    /// Rough estimate of character index in header (monospaced font)
    func headerCharacterIndex(at pointInItem: NSPoint) -> Int? {
        if headerLabel.isHidden { return nil }
        let frame = headerLabel.frame
        if !frame.contains(pointInItem) { return nil }
        let localX = pointInItem.x - frame.minX
        let text = headerLabel.stringValue
        guard !text.isEmpty else { return 0 }
        let width = frame.width
        if width <= 0 { return 0 }
        let approxCharWidth = width / CGFloat(max(1, text.count))
        let idx = Int(floor(localX / max(1, approxCharWidth)))
        return max(0, min(idx, text.count))
    }
}