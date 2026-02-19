import Cocoa

// MARK: - Global Document rebuild
extension TerminalViewController {
    /// Completely rebuilds global segment map starting from specified block index.
    /// For simplicity, currently recalculating entire document.
    func rebuildGlobalDocument(startingAt _: Int) {
        var segments: [GlobalSegment] = []
        var offset = 0
        for (idx, block) in commandBlocks.enumerated() {
            if let command = block.command {
                let cmdTextNSString = ("$ \(command)\n") as NSString
                let range = NSRange(location: offset, length: cmdTextNSString.length)
                segments.append(GlobalSegment(blockIndex: idx, kind: .header, range: range))
                offset += cmdTextNSString.length
            }

            if !block.outputText.isEmpty {
                let outNSString = block.outputText as NSString
                let range = NSRange(location: offset, length: outNSString.length)
                segments.append(GlobalSegment(blockIndex: idx, kind: .output, range: range))
                offset += outNSString.length
            }
        }
        globalDocument = GlobalDocument(totalLength: offset, segments: segments)
        // print("🧭 GlobalDocument rebuilt: length=\(globalDocument.totalLength), segments=\(globalDocument.segments.count)")
    }
}

// MARK: - Collection helpers
extension TerminalViewController: NSCollectionViewDataSource, NSCollectionViewDelegate, NSCollectionViewDelegateFlowLayout {
    func numberOfSections(in collectionView: NSCollectionView) -> Int { 1 }
    func collectionView(_ collectionView: NSCollectionView, numberOfItemsInSection section: Int) -> Int {
        return commandBlocks.count
    }

    func collectionView(_ collectionView: NSCollectionView, itemForRepresentedObjectAt indexPath: IndexPath) -> NSCollectionViewItem {
        let item = collectionView.makeItem(withIdentifier: CommandBlockItem.reuseId, for: indexPath)
        guard let blockItem = item as? CommandBlockItem else { return item }
        let block = commandBlocks[indexPath.item]
        blockItem.configure(commandId: block.commandId, command: block.command, outputSegments: block.outputSegments, activeScreenLines: block.activeScreenLines, isFinished: block.isFinished, exitCode: block.exitCode, cwdStart: block.cwdStart, serverHome: serverHome)
        blockItem.delegate = self
        // apply highlight for current selection if it intersects this block
        applySelectionHighlightIfNeeded(to: blockItem, at: indexPath.item)
        return blockItem
    }

    func collectionView(_ collectionView: NSCollectionView, layout collectionViewLayout: NSCollectionViewLayout, sizeForItemAt indexPath: IndexPath) -> NSSize {
        let contentWidth = collectionView.bounds.width - (collectionLayout.sectionInset.left + collectionLayout.sectionInset.right)
        let block = commandBlocks[indexPath.item]
        let height = CommandBlockItem.estimatedHeight(command: block.command, outputText: block.outputText, width: contentWidth, cwdStart: block.cwdStart)
        return NSSize(width: contentWidth, height: height)
    }

    func insertBlock(at index: Int) {
        let indexPath = IndexPath(item: index, section: 0)
        collectionView.performBatchUpdates({
            collectionView.insertItems(at: Set([indexPath]))
        }, completionHandler: { _ in
            self.updateTextViewFrame()
            self.scrollToBottom()
        })
    }

    func reloadBlock(at index: Int) {
        let indexPath = IndexPath(item: index, section: 0)
        collectionView.reloadItems(at: Set([indexPath]))
        self.updateTextViewFrame()
        scrollToBottomThrottled()
    }

    func scrollToBottom() {
        let count = collectionView.numberOfItems(inSection: 0)
        if count > 0 {
            let indexPath = IndexPath(item: count - 1, section: 0)
            collectionView.scrollToItems(at: Set([indexPath]), scrollPosition: .bottom)
        }
    }

    private var lastScrollUpdate: TimeInterval { get { _lastScrollUpdate } set { _lastScrollUpdate = newValue } }
    private static var _scrollTimestamp: TimeInterval = 0
    private var _lastScrollUpdate: TimeInterval {
        get { return TerminalViewController._scrollTimestamp }
        set { TerminalViewController._scrollTimestamp = newValue }
    }
    func scrollToBottomThrottled() {
        let now = CFAbsoluteTimeGetCurrent()
        if now - lastScrollUpdate > 0.03 {
            lastScrollUpdate = now
            scrollToBottom()
        }
    }
}

