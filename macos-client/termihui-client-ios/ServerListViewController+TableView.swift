import UIKit

// MARK: - UITableViewDataSource

extension ServerListViewController: UITableViewDataSource {
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return servers.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "ServerCell", for: indexPath)
        let address = servers[indexPath.row]
        
        var config = cell.defaultContentConfiguration()
        config.text = address
        config.image = UIImage(systemName: "server.rack")
        cell.contentConfiguration = config
        cell.accessoryType = .disclosureIndicator
        
        return cell
    }
}

// MARK: - UITableViewDelegate

extension ServerListViewController: UITableViewDelegate {
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        let address = servers[indexPath.row]
        delegate?.serverListViewController(self, didSelectServer: address)
    }
    
    func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
        if editingStyle == .delete {
            AppSettings.shared.removeServer(at: indexPath.row)
            servers.remove(at: indexPath.row)
            tableView.deleteRows(at: [indexPath], with: .automatic)
            updateEmptyState()
        }
    }
    
    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        return servers.isEmpty ? nil : "Saved Servers"
    }
}
