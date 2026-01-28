import Cocoa
import SnapKit

/// Custom view that can disable hit testing when not interactive
class SidebarContainerView: NSView {
    var isInteractive: Bool = false
    
    override func hitTest(_ point: NSPoint) -> NSView? {
        // Ignore all mouse events when not interactive
        guard isInteractive else { return nil }
        return super.hitTest(point)
    }
}

/// Protocol for session list controller
protocol SessionListViewController: AnyObject {
    var view: NSView { get }
    func updateSessions(_ sessions: [SessionInfo], activeId: UInt64)
    func setActiveSession(_ sessionId: UInt64)
    func setInteractive(_ interactive: Bool)
}

/// Delegate for session list actions
protocol SessionListViewControllerDelegate: AnyObject {
    func sessionListViewControllerDidRequestNewSession(_ controller: SessionListViewController)
    func sessionListViewController(_ controller: SessionListViewController, didSelectSession sessionId: UInt64)
    func sessionListViewController(_ controller: SessionListViewController, didRequestDeleteSession sessionId: UInt64)
}

/// Child view controller for the session sidebar
final class SessionListViewControllerImpl: NSViewController, SessionListViewController {
    
    // MARK: - Properties
    weak var delegate: SessionListViewControllerDelegate?
    
    private var sessions: [SessionInfo] = []
    private var activeSessionId: UInt64 = 0
    private var isProgrammaticSelection = false
    
    // MARK: - UI Components
    private let visualEffectView = NSVisualEffectView()
    private let addSessionButton = NSButton()
    private let outlineScrollView = NSScrollView()
    private let outlineView = NSOutlineView()
    
    // MARK: - Lifecycle
    override func loadView() {
        // Use custom view that ignores hits when hidden
        let sidebarView = SidebarContainerView()
        sidebarView.wantsLayer = true
        sidebarView.translatesAutoresizingMaskIntoConstraints = false
        // Start completely hidden
        sidebarView.alphaValue = 0
        sidebarView.isInteractive = false
        view = sidebarView
    }
    
    /// Enable/disable mouse interaction
    func setInteractive(_ interactive: Bool) {
        (view as? SidebarContainerView)?.isInteractive = interactive
    }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
    }
    
    // MARK: - Setup
    private func setupUI() {
        // Visual effect view for native macOS blur
        visualEffectView.translatesAutoresizingMaskIntoConstraints = false
        visualEffectView.material = .sidebar
        visualEffectView.blendingMode = .behindWindow
        visualEffectView.state = .active
        view.addSubview(visualEffectView)
        
        visualEffectView.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
        
        // Add session button at top
        addSessionButton.translatesAutoresizingMaskIntoConstraints = false
        addSessionButton.title = "+ Session"
        addSessionButton.bezelStyle = .rounded
        addSessionButton.target = self
        addSessionButton.action = #selector(addSessionClicked)
        visualEffectView.addSubview(addSessionButton)
        
        addSessionButton.snp.makeConstraints { make in
            make.top.equalToSuperview().offset(8)
            make.leading.equalToSuperview().offset(8)
            make.trailing.equalToSuperview().offset(-8)
            make.height.equalTo(24)
        }
        
        // Outline view for session list
        setupOutlineView()
        visualEffectView.addSubview(outlineScrollView)
        
        outlineScrollView.snp.makeConstraints { make in
            make.top.equalTo(addSessionButton.snp.bottom).offset(8)
            make.leading.trailing.bottom.equalToSuperview()
        }
    }
    
    private func setupOutlineView() {
        // Configure outline view
        outlineView.headerView = nil
        outlineView.rowHeight = 28
        outlineView.indentationPerLevel = 16
        outlineView.autoresizesOutlineColumn = true
        outlineView.selectionHighlightStyle = .sourceList
        outlineView.delegate = self
        outlineView.dataSource = self
        
        // Single column for session names
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SessionColumn"))
        column.title = "Sessions"
        column.minWidth = 100
        outlineView.addTableColumn(column)
        outlineView.outlineTableColumn = column
        
        // Scroll view setup
        outlineScrollView.translatesAutoresizingMaskIntoConstraints = false
        outlineScrollView.documentView = outlineView
        outlineScrollView.hasVerticalScroller = true
        outlineScrollView.autohidesScrollers = true
        outlineScrollView.drawsBackground = false
        outlineView.backgroundColor = .clear
    }
    
    // MARK: - Actions
    @objc private func addSessionClicked() {
        delegate?.sessionListViewControllerDidRequestNewSession(self)
    }
    
    // MARK: - Public API
    func updateSessions(_ sessions: [SessionInfo], activeId: UInt64) {
        self.sessions = sessions
        self.activeSessionId = activeId
        outlineView.reloadData()
        
        // Select active session (programmatically - don't trigger delegate)
        if let index = sessions.firstIndex(where: { $0.id == activeId }) {
            isProgrammaticSelection = true
            outlineView.selectRowIndexes(IndexSet(integer: index), byExtendingSelection: false)
            isProgrammaticSelection = false
        }
    }
    
    func setActiveSession(_ sessionId: UInt64) {
        activeSessionId = sessionId
        outlineView.reloadData()
        
        if let index = sessions.firstIndex(where: { $0.id == sessionId }) {
            isProgrammaticSelection = true
            outlineView.selectRowIndexes(IndexSet(integer: index), byExtendingSelection: false)
            isProgrammaticSelection = false
        }
    }
}

// MARK: - NSOutlineViewDataSource
extension SessionListViewControllerImpl: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        // Root level - all sessions
        if item == nil {
            return sessions.count
        }
        // Sessions don't have children (yet)
        return 0
    }
    
    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        if item == nil {
            return sessions[index]
        }
        fatalError("Unexpected item")
    }
    
    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        return false // Sessions are leaf nodes for now
    }
}

// MARK: - NSOutlineViewDelegate
extension SessionListViewControllerImpl: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        guard let session = item as? SessionInfo else { return nil }
        
        let cellIdentifier = NSUserInterfaceItemIdentifier("SessionCell")
        
        var cellView = outlineView.makeView(withIdentifier: cellIdentifier, owner: self) as? SessionCellView
        if cellView == nil {
            cellView = SessionCellView()
            cellView?.identifier = cellIdentifier
        }
        
        cellView?.configure(sessionId: session.id, isActive: session.id == activeSessionId)
        cellView?.onDelete = { [weak self] in
            guard let self = self else { return }
            self.delegate?.sessionListViewController(self, didRequestDeleteSession: session.id)
        }
        
        return cellView
    }
    
    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard !isProgrammaticSelection else { return }
        
        let row = outlineView.selectedRow
        guard row >= 0, row < sessions.count else { return }
        
        let session = sessions[row]
        delegate?.sessionListViewController(self, didSelectSession: session.id)
    }
}

