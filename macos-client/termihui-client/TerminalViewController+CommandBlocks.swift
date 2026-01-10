import Cocoa

// MARK: - Selection Gestures Setup
extension TerminalViewController {
    func setupSelectionGestures() {
        // Перехватим события колллекции, чтобы мышь шла через VC
        collectionView.postsFrameChangedNotifications = true
        collectionView.acceptsTouchEvents = false
        // Включаем отслеживание мыши
        collectionView.addTrackingArea(NSTrackingArea(rect: collectionView.bounds, options: [.activeAlways, .mouseMoved, .mouseEnteredAndExited, .inVisibleRect], owner: self, userInfo: nil))
        
        // Жест нажатия (эмулирует mouseDown)
        let press = NSPressGestureRecognizer(target: self, action: #selector(handlePressGesture(_:)))
        press.minimumPressDuration = 0
        press.delegate = self
        collectionView.addGestureRecognizer(press)
        
        // Жест перетаскивания (эмулирует mouseDragged)
        let pan = NSPanGestureRecognizer(target: self, action: #selector(handlePanGesture(_:)))
        pan.delegate = self
        collectionView.addGestureRecognizer(pan)
    }
}

// MARK: - Command Block Events
extension TerminalViewController {
    // Пока просто фиксация событий, без изменения текста.
    func didStartCommandBlock(command: String? = nil, cwd: String? = nil) {
        // print("🧱 Started command block: \(command ?? "<unknown>"), cwd: \(cwd ?? "<unknown>")")
        let block = CommandBlock(commandId: nil, command: command, outputSegments: [], isFinished: false, exitCode: nil, cwdStart: cwd, cwdEnd: nil)
        commandBlocks.append(block)
        currentBlockIndex = commandBlocks.count - 1
        insertBlock(at: currentBlockIndex!)
        rebuildGlobalDocument(startingAt: currentBlockIndex!)
        
        // Enter raw input mode for interactive commands
        enterRawInputMode()
    }
    
    func didFinishCommandBlock(exitCode: Int, cwd: String? = nil) {
        // print("🏁 Finished command block (exit=\(exitCode)), cwd: \(cwd ?? "<unknown>")")
        if let idx = currentBlockIndex {
            commandBlocks[idx].isFinished = true
            commandBlocks[idx].exitCode = exitCode
            commandBlocks[idx].cwdEnd = cwd
            reloadBlock(at: idx)
            currentBlockIndex = nil
            rebuildGlobalDocument(startingAt: idx)
        }
        // Обновляем отображение cwd если он изменился (например после cd)
        if let newCwd = cwd {
            updateCurrentCwd(newCwd)
        }
        
        // Exit raw input mode
        exitRawInputMode()
    }
    
    /// Loads command history from server
    func loadHistory(_ history: [CommandHistoryRecord]) {
        // print("📜 Loading history: \(history.count) commands")
        
        // Clear current state
        commandBlocks.removeAll()
        currentBlockIndex = nil
        globalDocument = GlobalDocument()
        selectionRange = nil
        selectionAnchor = nil
        
        // Create blocks from history
        for record in history {
            let block = CommandBlock(
                commandId: record.id,
                command: record.command.isEmpty ? nil : record.command,
                outputSegments: record.segments,
                isFinished: record.isFinished,
                exitCode: record.exitCode,
                cwdStart: record.cwdStart.isEmpty ? nil : record.cwdStart,
                cwdEnd: record.cwdEnd.isEmpty ? nil : record.cwdEnd
            )
            commandBlocks.append(block)
        }
        
        // Полная перезагрузка collectionView после обновления модели
        collectionView.reloadData()
        
        if !commandBlocks.isEmpty {
            rebuildGlobalDocument(startingAt: 0)
            
            // Check if last block is unfinished (running command)
            // If so, set currentBlockIndex to continue appending output to it
            if let lastIndex = commandBlocks.indices.last, !commandBlocks[lastIndex].isFinished {
                currentBlockIndex = lastIndex
                // print("📜 Resuming unfinished command block at index \(lastIndex)")
                enterRawInputMode()
            }
            
            // Update CWD from last finished block
            if let lastBlock = commandBlocks.last {
                if let cwd = lastBlock.cwdEnd ?? lastBlock.cwdStart {
                    updateCurrentCwd(cwd)
                }
            }
            
            // Scroll to bottom
            DispatchQueue.main.async {
                self.updateTextViewFrame()
                self.scrollToBottom()
            }
        }
        
        // print("📜 History loaded")
    }
}

