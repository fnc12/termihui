import Cocoa

// MARK: - Selection handling & highlighting
extension TerminalViewController {
    override func mouseDown(with event: NSEvent) {
        guard view.window != nil else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        guard let (_, localIndex) = hitTestGlobalIndex(at: locationInView) else { return }
        let globalIndex = localIndex
        isSelecting = true
        selectionAnchor = globalIndex
        selectionRange = NSRange(location: globalIndex, length: 0)
        updateSelectionHighlight()
    }

    override func mouseDragged(with event: NSEvent) {
        guard isSelecting, let anchor = selectionAnchor else { return }
        let locationInView = view.convert(event.locationInWindow, from: nil)
        
        // Autoscroll if cursor is outside scroll view bounds
        let scrollBounds = terminalScrollView.convert(terminalScrollView.bounds, to: view)
        handleAutoscroll(locationInView: locationInView, scrollBounds: scrollBounds, event: event)
        
        // Get global index - use edge detection if outside content
        let globalIndex: Int
        if let (_, idx) = hitTestGlobalIndex(at: locationInView) {
            globalIndex = idx
        } else {
            // Cursor is outside content - select to edge
            globalIndex = getEdgeGlobalIndex(locationInView: locationInView, scrollBounds: scrollBounds)
        }
        
        let start = min(anchor, globalIndex)
        let end = max(anchor, globalIndex)
        selectionRange = NSRange(location: start, length: end - start)
        updateSelectionHighlight()
    }

    override func mouseUp(with event: NSEvent) {
        isSelecting = false
        stopAutoscrollTimer()
    }
    
    // MARK: - Autoscroll Support
    
    private func handleAutoscroll(locationInView: NSPoint, scrollBounds: NSRect, event: NSEvent) {
        lastDragEvent = event
        
        // Calculate distance outside scroll bounds
        var deltaY: CGFloat = 0
        if locationInView.y < scrollBounds.minY {
            deltaY = locationInView.y - scrollBounds.minY // negative = scroll down (content moves up)
        } else if locationInView.y > scrollBounds.maxY {
            deltaY = locationInView.y - scrollBounds.maxY // positive = scroll up (content moves down)
        }
        
        if abs(deltaY) > 0 {
            // Start autoscroll timer if not running
            if autoscrollTimer == nil {
                autoscrollTimer = Timer.scheduledTimer(withTimeInterval: 0.02, repeats: true) { [weak self] _ in
                    self?.performAutoscroll()
                }
            }
        } else {
            stopAutoscrollTimer()
        }
    }
    
    private func performAutoscroll() {
        guard isSelecting, let event = lastDragEvent else {
            stopAutoscrollTimer()
            return
        }
        
        let locationInView = view.convert(event.locationInWindow, from: nil)
        let scrollBounds = terminalScrollView.convert(terminalScrollView.bounds, to: view)
        
        // Calculate scroll speed proportional to distance
        var deltaY: CGFloat = 0
        if locationInView.y < scrollBounds.minY {
            deltaY = (scrollBounds.minY - locationInView.y) * 0.5 // scroll down
        } else if locationInView.y > scrollBounds.maxY {
            deltaY = (scrollBounds.maxY - locationInView.y) * 0.5 // scroll up (negative)
        }
        
        if abs(deltaY) < 1 {
            stopAutoscrollTimer()
            return
        }
        
        // Apply scroll
        let clipView = terminalScrollView.contentView
        var newOrigin = clipView.bounds.origin
        newOrigin.y = max(0, min(newOrigin.y + deltaY, collectionView.frame.height - clipView.bounds.height))
        clipView.setBoundsOrigin(newOrigin)
        terminalScrollView.reflectScrolledClipView(clipView)
        
        // Update selection with new scroll position
        if let anchor = selectionAnchor {
            let globalIndex: Int
            if let (_, idx) = hitTestGlobalIndex(at: locationInView) {
                globalIndex = idx
            } else {
                globalIndex = getEdgeGlobalIndex(locationInView: locationInView, scrollBounds: scrollBounds)
            }
            let start = min(anchor, globalIndex)
            let end = max(anchor, globalIndex)
            selectionRange = NSRange(location: start, length: end - start)
            updateSelectionHighlight()
        }
    }
    
    private func stopAutoscrollTimer() {
        autoscrollTimer?.invalidate()
        autoscrollTimer = nil
    }
    
    /// Returns global index at document edge when cursor is outside content
    private func getEdgeGlobalIndex(locationInView: NSPoint, scrollBounds: NSRect) -> Int {
        if locationInView.y < scrollBounds.minY {
            // Below scroll view (in flipped coordinates this means end of document)
            return globalDocument.totalLength
        } else if locationInView.y > scrollBounds.maxY {
            // Above scroll view (beginning of document)
            return 0
        }
        // Horizontal edges - find nearest visible line
        return selectionAnchor ?? 0
    }

    override func keyDown(with event: NSEvent) {
        // Cmd+C — copy selected text (works in both modes)
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "c" {
            copySelectionToPasteboard()
            return
        }
        
        // Cmd+V — paste (in raw mode, send to PTY)
        if event.modifierFlags.contains(.command), let chars = event.charactersIgnoringModifiers, chars.lowercased() == "v" {
            if isCommandRunning {
                if let pasteString = NSPasteboard.general.string(forType: .string) {
                    sendRawInput(pasteString)
                }
                return
            }
        }
        
        // Raw input mode: send all keypresses to PTY
        if isCommandRunning {
            handleRawKeyDown(event)
            return
        }
        
        super.keyDown(with: event)
    }
    
    /// Handles key press in raw input mode
    private func handleRawKeyDown(_ event: NSEvent) {
        let keyCode = event.keyCode
        let modifiers = event.modifierFlags
        
        // Handle special keys
        switch keyCode {
        case 36: // Enter/Return
            sendRawInput("\n")
            return
        case 51: // Backspace
            sendRawInput("\u{7f}") // DEL character
            return
        case 53: // Escape
            sendRawInput("\u{1b}")
            return
        case 48: // Tab
            sendRawInput("\t")
            return
        case 123: // Left arrow
            sendRawInput("\u{1b}[D")
            return
        case 124: // Right arrow
            sendRawInput("\u{1b}[C")
            return
        case 125: // Down arrow
            sendRawInput("\u{1b}[B")
            return
        case 126: // Up arrow
            sendRawInput("\u{1b}[A")
            return
        case 115: // Home
            sendRawInput("\u{1b}[H")
            return
        case 119: // End
            sendRawInput("\u{1b}[F")
            return
        case 116: // Page Up
            sendRawInput("\u{1b}[5~")
            return
        case 121: // Page Down
            sendRawInput("\u{1b}[6~")
            return
        case 117: // Delete (forward)
            sendRawInput("\u{1b}[3~")
            return
        default:
            break
        }
        
        // Ctrl+key combinations
        if modifiers.contains(.control), let chars = event.charactersIgnoringModifiers {
            if let char = chars.first {
                let asciiValue = char.asciiValue ?? 0
                // Ctrl+A = 1, Ctrl+B = 2, ..., Ctrl+Z = 26
                if asciiValue >= 97 && asciiValue <= 122 { // a-z
                    let ctrlChar = Character(UnicodeScalar(asciiValue - 96))
                    sendRawInput(String(ctrlChar))
                    return
                }
                // Ctrl+C specifically
                if char == "c" {
                    sendRawInput("\u{03}") // ETX (Ctrl+C)
                    return
                }
                // Ctrl+D
                if char == "d" {
                    sendRawInput("\u{04}") // EOT (Ctrl+D)
                    return
                }
                // Ctrl+Z
                if char == "z" {
                    sendRawInput("\u{1a}") // SUB (Ctrl+Z)
                    return
                }
            }
        }
        
        // Regular characters
        if let chars = event.characters, !chars.isEmpty {
            sendRawInput(chars)
        }
    }

    /// Converts click coordinate to global character index if it hits text
    func hitTestGlobalIndex(at pointInRoot: NSPoint) -> (blockIndex: Int, globalIndex: Int)? {
        // Iterate through visible items
        let visible = collectionView.visibleItems()
        for case let item as CommandBlockItem in visible {
            guard let indexPath = collectionView.indexPath(for: item) else { continue }
            // Convert point to item coordinates
            let pointInItem = item.view.convert(pointInRoot, from: view)
            if !item.view.bounds.contains(pointInItem) { continue }

            // Check header
            if let hIdx = item.headerCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .header, localIndex: hIdx)
                return (indexPath.item, global)
            }
            // Check body
            if let bIdx = item.bodyCharacterIndex(at: pointInItem) {
                let global = mapLocalToGlobal(blockIndex: indexPath.item, kind: .output, localIndex: bIdx)
                return (indexPath.item, global)
            }
        }
        return nil
    }

    /// Converts local character index within block to global document index
    func mapLocalToGlobal(blockIndex: Int, kind: SegmentKind, localIndex: Int) -> Int {
        for seg in globalDocument.segments {
            if seg.blockIndex == blockIndex && seg.kind == kind {
                return seg.range.location + min(localIndex, seg.range.length)
            }
        }
        // if segment not found — return end of document
        return globalDocument.totalLength
    }

    /// Highlights current selection in all visible cells
    func updateSelectionHighlight() {
        guard let sel = selectionRange else {
            // clear highlight
            for case let item as CommandBlockItem in collectionView.visibleItems() {
                item.clearSelectionHighlight()
            }
            return
        }
        for case let item as CommandBlockItem in collectionView.visibleItems() {
            guard let indexPath = collectionView.indexPath(for: item) else { continue }
            let headerLocal = localRange(for: sel, blockIndex: indexPath.item, kind: .header)
            let bodyLocal = localRange(for: sel, blockIndex: indexPath.item, kind: .output)
            item.setSelectionHighlight(headerRange: headerLocal, bodyRange: bodyLocal)
        }
    }

    /// Returns local range within specified segment for global selection range
    func localRange(for global: NSRange, blockIndex: Int, kind: SegmentKind) -> NSRange? {
        guard let seg = globalDocument.segments.first(where: { $0.blockIndex == blockIndex && $0.kind == kind }) else { return nil }
        let inter = intersection(of: global, and: seg.range)
        guard inter.length > 0 else { return nil }
        return NSRange(location: inter.location - seg.range.location, length: inter.length)
    }

    func intersection(of a: NSRange, and b: NSRange) -> NSRange {
        let start = max(a.location, b.location)
        let end = min(a.location + a.length, b.location + b.length)
        return end > start ? NSRange(location: start, length: end - start) : NSRange(location: 0, length: 0)
    }

    /// Applies highlight when configuring cell
    func applySelectionHighlightIfNeeded(to item: CommandBlockItem, at blockIndex: Int) {
        guard let sel = selectionRange else {
            item.clearSelectionHighlight(); return
        }
        let headerLocal = localRange(for: sel, blockIndex: blockIndex, kind: .header)
        let bodyLocal = localRange(for: sel, blockIndex: blockIndex, kind: .output)
        item.setSelectionHighlight(headerRange: headerLocal, bodyRange: bodyLocal)
    }

    func copySelectionToPasteboard() {
        guard let sel = selectionRange, sel.length > 0 else { return }
        var result = ""
        for seg in globalDocument.segments {
            let inter = intersection(of: sel, and: seg.range)
            guard inter.length > 0 else { continue }
            let local = NSRange(location: inter.location - seg.range.location, length: inter.length)
            let block = commandBlocks[seg.blockIndex]
            switch seg.kind {
            case .header:
                let ns = ("$ \(block.command ?? "")\n") as NSString
                if local.location < ns.length, local.length > 0, local.location + local.length <= ns.length {
                    var piece = ns.substring(with: local)
                    if piece.hasPrefix("$ ") { piece.removeFirst(2) }
                    result += piece
                }
            case .output:
                let ns = (block.outputText as NSString)
                if local.location < ns.length, local.length > 0, local.location + local.length <= ns.length {
                    result += ns.substring(with: local)
                }
            }
        }
        let pb = NSPasteboard.general
        pb.clearContents()
        pb.setString(result, forType: .string)
    }
}

// MARK: - Gesture handlers
extension TerminalViewController {
    @objc func handlePressGesture(_ gr: NSPressGestureRecognizer) {
        guard let v = gr.view else { return }
        let p = view.convert(gr.location(in: v), from: v)
        switch gr.state {
        case .began:
            // Назначаем себя firstResponder, чтобы перехватывать Cmd+C
            view.window?.makeFirstResponder(self)
            if let (_, gi) = hitTestGlobalIndex(at: p) {
                isSelecting = true
                selectionAnchor = gi
                selectionRange = NSRange(location: gi, length: 0)
                updateSelectionHighlight()
            }
        default:
            break
        }
    }
    
    @objc func handlePanGesture(_ gr: NSPanGestureRecognizer) {
        guard let v = gr.view else { return }
        let p = view.convert(gr.location(in: v), from: v)
        switch gr.state {
        case .began:
            // Назначаем себя firstResponder, чтобы перехватывать Cmd+C
            view.window?.makeFirstResponder(self)
            if let (_, gi) = hitTestGlobalIndex(at: p) {
                isSelecting = true
                selectionAnchor = gi
                selectionRange = NSRange(location: gi, length: 0)
                updateSelectionHighlight()
            }
        case .changed:
            guard isSelecting, let anchor = selectionAnchor, let (_, gi) = hitTestGlobalIndex(at: p) else { return }
            let start = min(anchor, gi)
            let end = max(anchor, gi)
            selectionRange = NSRange(location: start, length: end - start)
            updateSelectionHighlight()
        case .ended, .cancelled, .failed:
            isSelecting = false
        default:
            break
        }
    }
}

// MARK: - Standard Edit Actions
extension TerminalViewController {
    @IBAction func copy(_ sender: Any?) {
        if let sel = selectionRange, sel.length > 0 {
            copySelectionToPasteboard()
        } else {
            NSSound.beep()
        }
    }
}

// MARK: - NSGestureRecognizerDelegate
extension TerminalViewController {
    func gestureRecognizer(_ gestureRecognizer: NSGestureRecognizer, shouldRecognizeSimultaneouslyWith otherGestureRecognizer: NSGestureRecognizer) -> Bool {
        return true
    }
}

