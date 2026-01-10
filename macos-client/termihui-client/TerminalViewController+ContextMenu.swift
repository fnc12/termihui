import Foundation

extension TerminalViewController: CommandBlockItemDelegate {
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyAll commandId: UInt64?) {
        guard let clientCore else {
            print("[TerminalViewController] Error: clientCore is nil in didRequestCopyAll")
            return
        }
        var message: [String: Any] = ["type": "copyBlock", "copyType": "all"]
        if let commandId {
            message["commandId"] = commandId
        }
        clientCore.send(message)
    }
    
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyCommand commandId: UInt64?) {
        guard let clientCore else {
            print("[TerminalViewController] Error: clientCore is nil in didRequestCopyCommand")
            return
        }
        var message: [String: Any] = ["type": "copyBlock", "copyType": "command"]
        if let commandId {
            message["commandId"] = commandId
        }
        clientCore.send(message)
    }
    
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyOutput commandId: UInt64?) {
        guard let clientCore else {
            print("[TerminalViewController] Error: clientCore is nil in didRequestCopyOutput")
            return
        }
        var message: [String: Any] = ["type": "copyBlock", "copyType": "output"]
        if let commandId {
            message["commandId"] = commandId
        }
        clientCore.send(message)
    }
}
