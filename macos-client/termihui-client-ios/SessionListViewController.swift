import UIKit
import SnapKit

protocol SessionListViewControllerDelegate: AnyObject {
    func sessionListViewControllerDidRequestDisconnect(_ controller: SessionListViewController)
    func sessionListViewController(_ controller: SessionListViewController, didSelectSession sessionId: UInt64)
}

/// Session list screen for connected server
class SessionListViewController: UIViewController {
    
    // MARK: - UI
    private let tableView = UITableView(frame: .zero, style: .insetGrouped)
    private let statusLabel = UILabel()
    private let activityIndicator = UIActivityIndicatorView(style: .medium)
    
    // MARK: - Properties
    var serverAddress: String = ""
    weak var delegate: SessionListViewControllerDelegate?
    var sessions: [SessionInfo] = []
    var activeSessionId: UInt64 = 0
    var isConnected = false
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        showConnecting()
    }
    
    override func viewWillDisappear(_ animated: Bool) {
        super.viewWillDisappear(animated)
        
        // Disconnect when going back (pop)
        if isMovingFromParent {
            delegate?.sessionListViewControllerDidRequestDisconnect(self)
        }
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        title = serverAddress
        view.backgroundColor = .systemGroupedBackground
        
        // Status area
        let statusStack = UIStackView(arrangedSubviews: [activityIndicator, statusLabel])
        statusStack.axis = .horizontal
        statusStack.spacing = 8
        statusStack.alignment = .center
        
        statusLabel.textColor = .secondaryLabel
        statusLabel.font = .systemFont(ofSize: 14)
        
        // TableView
        tableView.delegate = self
        tableView.dataSource = self
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "SessionCell")
        tableView.tableHeaderView = createHeaderView(with: statusStack)
        
        view.addSubview(tableView)
        
        tableView.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
    }
    
    private func createHeaderView(with statusStack: UIStackView) -> UIView {
        let header = UIView(frame: CGRect(x: 0, y: 0, width: 0, height: 44))
        header.addSubview(statusStack)
        
        statusStack.snp.makeConstraints { make in
            make.center.equalToSuperview()
        }
        
        return header
    }
    
    // MARK: - Public Methods
    
    func showConnecting() {
        isConnected = false
        activityIndicator.startAnimating()
        statusLabel.text = "Connecting..."
        sessions = []
        tableView.reloadData()
    }
    
    func showConnected() {
        isConnected = true
        activityIndicator.stopAnimating()
        statusLabel.text = "Connected"
    }
    
    func updateSessions(_ newSessions: [SessionInfo], activeId: UInt64? = nil) {
        sessions = newSessions
        if let activeId = activeId {
            activeSessionId = activeId
        }
        tableView.reloadData()
        updateEmptyState()
    }
    
    private func updateEmptyState() {
        if sessions.isEmpty && isConnected {
            let label = UILabel()
            label.text = "No active sessions"
            label.textAlignment = .center
            label.textColor = .secondaryLabel
            tableView.backgroundView = label
        } else {
            tableView.backgroundView = nil
        }
    }
}
