import Foundation

protocol TerminalViewControllerDelegate: AnyObject {
    func terminalViewController(_ controller: TerminalViewController, didSendCommand command: String)
    func terminalViewControllerDidRequestDisconnect(_ controller: TerminalViewController)
}

