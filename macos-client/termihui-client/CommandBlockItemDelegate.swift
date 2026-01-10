import Foundation

protocol CommandBlockItemDelegate: AnyObject {
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyAll commandId: UInt64?)
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyCommand commandId: UInt64?)
    func commandBlockItem(_ item: CommandBlockItem, didRequestCopyOutput commandId: UInt64?)
}
