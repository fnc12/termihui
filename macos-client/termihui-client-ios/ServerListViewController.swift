import UIKit
import SnapKit

protocol ServerListViewControllerDelegate: AnyObject {
    func serverListViewController(_ controller: ServerListViewController, didSelectServer address: String)
}

/// Server list screen
class ServerListViewController: UIViewController {
    
    // MARK: - UI
    private let tableView = UITableView(frame: .zero, style: .insetGrouped)
    
    // MARK: - Properties
    weak var delegate: ServerListViewControllerDelegate?
    var servers: [String] = []
    
    // MARK: - Lifecycle
    
    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        loadServers()
    }
    
    override func viewWillAppear(_ animated: Bool) {
        super.viewWillAppear(animated)
        loadServers()
    }
    
    // MARK: - Setup
    
    private func setupUI() {
        title = "Servers"
        view.backgroundColor = .systemGroupedBackground
        
        // Navigation bar
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .add,
            target: self,
            action: #selector(addServerTapped)
        )
        
        // TableView
        tableView.delegate = self
        tableView.dataSource = self
        tableView.register(UITableViewCell.self, forCellReuseIdentifier: "ServerCell")
        view.addSubview(tableView)
        
        tableView.snp.makeConstraints { make in
            make.edges.equalToSuperview()
        }
    }
    
    private func loadServers() {
        servers = AppSettings.shared.servers
        tableView.reloadData()
        updateEmptyState()
    }
    
    func updateEmptyState() {
        if servers.isEmpty {
            let label = UILabel()
            label.text = "No servers.\nTap + to add one."
            label.textAlignment = .center
            label.textColor = .secondaryLabel
            label.numberOfLines = 0
            tableView.backgroundView = label
        } else {
            tableView.backgroundView = nil
        }
    }
    
    // MARK: - Actions
    
    @objc private func addServerTapped() {
        let alert = UIAlertController(
            title: "Add Server",
            message: "Enter server address",
            preferredStyle: .alert
        )
        
        alert.addTextField { textField in
            textField.placeholder = "192.168.1.100:37854"
            textField.keyboardType = .numbersAndPunctuation
            textField.autocorrectionType = .no
            textField.autocapitalizationType = .none
        }
        
        alert.addAction(UIAlertAction(title: "Cancel", style: .cancel))
        alert.addAction(UIAlertAction(title: "Add", style: .default) { [weak self] _ in
            guard var address = alert.textFields?.first?.text?.trimmingCharacters(in: .whitespacesAndNewlines),
                  !address.isEmpty else { return }
            
            // Add default port if not specified
            if !address.contains(":") {
                address += ":37854"
            }
            
            AppSettings.shared.addServer(address)
            self?.loadServers()
        })
        
        present(alert, animated: true)
    }
}
