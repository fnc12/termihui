import UIKit

// MARK: - UITableViewDataSource

extension SessionListViewController: UITableViewDataSource {
    
    func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        return sessions.count
    }
    
    func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "SessionCell", for: indexPath)
        let session = sessions[indexPath.row]
        
        var config = cell.defaultContentConfiguration()
        config.text = "Session #\(session.id)"
        config.secondaryText = formatDate(session.createdAt)
        config.image = UIImage(systemName: "terminal")
        cell.contentConfiguration = config
        
        return cell
    }
    
    private func formatDate(_ timestamp: Int64) -> String {
        // Server sends timestamp in milliseconds
        let date = Date(timeIntervalSince1970: TimeInterval(timestamp) / 1000.0)
        let formatter = DateFormatter()
        formatter.dateStyle = .short
        formatter.timeStyle = .short
        return formatter.string(from: date)
    }
}

// MARK: - UITableViewDelegate

extension SessionListViewController: UITableViewDelegate {
    
    func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        // TODO: Open terminal session in the future
    }
    
    func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        return sessions.isEmpty ? nil : "Active Sessions"
    }
}
