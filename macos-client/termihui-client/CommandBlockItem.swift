import Cocoa

final class CommandBlockItem: NSCollectionViewItem {
    static let reuseId = NSUserInterfaceItemIdentifier("CommandBlockItem")
    
    /// Horizontal padding for content (left + right)
    static let horizontalPadding: CGFloat = 8
    
    private let cwdLabel = NSTextField(labelWithString: "")
    private let headerLabel = NSTextField(labelWithString: "")
    private let bodyTextView = NSTextView()
    private let container = NSView()
    private let separatorView = NSView()
    private let ansiParser = ANSIParser()
    
    // Current highlight state (for reset)
    private var lastHeaderHighlight: NSRange?
    private var hasBodyHighlight: Bool = false
    
    override func loadView() {
        view = container
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }
    
    func configure(command: String?, output: String, isFinished: Bool, exitCode: Int?, cwdStart: String?) {
        // CWD label — gray, above command
        if let cwd = cwdStart, !cwd.isEmpty {
            cwdLabel.stringValue = shortenHomePath(cwd)
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
        
        // Parse ANSI escape codes and apply styling
        let styledSegments = ansiParser.parse(output)
        let attributedOutput = styledSegments.toAttributedString()
        bodyTextView.textStorage?.setAttributedString(attributedOutput)
        clearSelectionHighlight()
    }
    
    /// Shortens home directory to ~
    private func shortenHomePath(_ path: String) -> String {
        if let pw = getpwuid(getuid()), let home = pw.pointee.pw_dir {
            let homeDir = String(cString: home)
            if path.hasPrefix(homeDir) {
                return "~" + String(path.dropFirst(homeDir.count))
            }
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
            bodyTextView.bottomAnchor.constraint(equalTo: separatorView.topAnchor, constant: -padding),

            separatorView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            separatorView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            separatorView.bottomAnchor.constraint(equalTo: view.bottomAnchor),
            separatorView.heightAnchor.constraint(equalToConstant: 1)
        ])
    }
    
    static func estimatedHeight(command: String?, output: String, width: CGFloat, cwdStart: String? = nil) -> CGFloat {
        let horizontalInsets: CGFloat = 16
        let constrainedWidth = max(0, width - horizontalInsets)
        
        var total: CGFloat = 16 // vertical insets
        
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
        // Rough estimate of body height
        let bodyRect = (output as NSString).boundingRect(
            with: NSSize(width: constrainedWidth, height: .greatestFiniteMagnitude),
            options: [.usesLineFragmentOrigin, .usesFontLeading],
            attributes: [.font: NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)]
        )
        total += ceil(bodyRect.height)
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


