import Cocoa

/// Custom view that can disable hit testing when not interactive
class ChatSidebarContainerView: NSView {
    var isInteractive: Bool = false
    
    override func hitTest(_ point: NSPoint) -> NSView? {
        guard isInteractive else { return nil }
        return super.hitTest(point)
    }
}
