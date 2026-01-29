import Foundation

/// Error type for message decoding failures
enum MessageDecodingError: Error {
    case keyNotFound(key: String, path: String)
    case typeMismatch(expectedType: String, path: String)
    case valueNotFound(valueType: String, path: String)
    case dataCorrupted(path: String)
    case jsonSerializationError(Error)
    case unknown(Error)
    
    var description: String {
        switch self {
        case .keyNotFound(let key, let path):
            return "Missing key '\(key)'\(path.isEmpty ? "" : " at path: \(path)")"
        case .typeMismatch(let expectedType, let path):
            return "Type mismatch: expected \(expectedType)\(path.isEmpty ? "" : " at path: \(path)")"
        case .valueNotFound(let valueType, let path):
            return "Value not found: \(valueType)\(path.isEmpty ? "" : " at path: \(path)")"
        case .dataCorrupted(let path):
            return "Data corrupted\(path.isEmpty ? "" : " at path: \(path)")"
        case .jsonSerializationError(let error):
            return "JSON serialization error: \(error)"
        case .unknown(let error):
            return "Unknown error: \(error)"
        }
    }
}

/// Service for decoding JSON messages
final class MessageDecoder {
    
    /// Decode JSON data to a Codable type
    /// - Parameters:
    ///   - type: The type to decode to
    ///   - data: Raw JSON object (from JSONSerialization)
    /// - Returns: Result with decoded object or MessageDecodingError
    func decode<T: Decodable>(_ type: T.Type, from data: Any) -> Result<T, MessageDecodingError> {
        do {
            let jsonData = try JSONSerialization.data(withJSONObject: data)
            let decoded = try JSONDecoder().decode(type, from: jsonData)
            return .success(decoded)
        } catch let error as DecodingError {
            return .failure(mapDecodingError(error))
        } catch {
            return .failure(.jsonSerializationError(error))
        }
    }
    
    private func mapDecodingError(_ error: DecodingError) -> MessageDecodingError {
        switch error {
        case .keyNotFound(let key, let ctx):
            return .keyNotFound(key: key.stringValue, path: codingPathString(ctx.codingPath))
        case .typeMismatch(let expectedType, let ctx):
            return .typeMismatch(expectedType: String(describing: expectedType), path: codingPathString(ctx.codingPath))
        case .valueNotFound(let valueType, let ctx):
            return .valueNotFound(valueType: String(describing: valueType), path: codingPathString(ctx.codingPath))
        case .dataCorrupted(let ctx):
            return .dataCorrupted(path: codingPathString(ctx.codingPath))
        @unknown default:
            return .unknown(error)
        }
    }
    
    private func codingPathString(_ path: [CodingKey]) -> String {
        path.map { $0.stringValue }.joined(separator: ".")
    }
}
