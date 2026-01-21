import Cocoa

// MARK: - NSTableViewDataSource
extension ChatSidebarViewController: NSTableViewDataSource {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return messagesCount
    }
}

// MARK: - NSTableViewDelegate
extension ChatSidebarViewController: NSTableViewDelegate {
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let message = getMessage(at: row)
        
        let cellIdentifier = NSUserInterfaceItemIdentifier("ChatMessageCell")
        var cellView = tableView.makeView(withIdentifier: cellIdentifier, owner: self) as? ChatMessageCellView
        
        if cellView == nil {
            cellView = ChatMessageCellView()
            cellView?.identifier = cellIdentifier
        }
        
        cellView?.configure(with: message)
        return cellView
    }
}
