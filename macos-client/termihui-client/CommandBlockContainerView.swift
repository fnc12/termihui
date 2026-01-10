import Cocoa

final class CommandBlockContainerView: NSView {
    weak var item: CommandBlockItem?
    
    override func menu(for event: NSEvent) -> NSMenu? {
        return menu
    }
}
