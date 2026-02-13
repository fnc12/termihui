import Cocoa

// MARK: - NSTableViewDataSource

extension LLMProvidersViewController: NSTableViewDataSource {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return providers.count
    }
}

// MARK: - NSTableViewDelegate

extension LLMProvidersViewController: NSTableViewDelegate {
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard row < providers.count else { return nil }
        let provider = providers[row]
        
        let identifier = tableColumn?.identifier ?? NSUserInterfaceItemIdentifier("")
        let cellId = NSUserInterfaceItemIdentifier("Cell_\(identifier.rawValue)")
        
        let textField: NSTextField
        if let existing = tableView.makeView(withIdentifier: cellId, owner: self) as? NSTextField {
            textField = existing
        } else {
            textField = NSTextField(labelWithString: "")
            textField.identifier = cellId
            textField.lineBreakMode = .byTruncatingTail
        }
        
        switch identifier.rawValue {
        case "NameColumn":
            textField.stringValue = provider.name
            textField.font = NSFont.systemFont(ofSize: 13, weight: .medium)
        case "URLColumn":
            textField.stringValue = provider.url
            textField.font = NSFont.systemFont(ofSize: 13)
            textField.textColor = .secondaryLabelColor
        case "ModelColumn":
            textField.stringValue = provider.model.isEmpty ? "---" : provider.model
            textField.font = NSFont.systemFont(ofSize: 13)
            textField.textColor = .secondaryLabelColor
        default:
            break
        }
        
        return textField
    }
    
    func tableViewSelectionDidChange(_ notification: Notification) {
        updateButtonStates()
    }
}
