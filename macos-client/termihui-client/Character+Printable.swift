import Foundation

extension Character {
    /// Returns true if character is printable (not a control character)
    var isPrintable: Bool {
        // Control characters are 0x00-0x1F and 0x7F
        guard let scalar = unicodeScalars.first else { return false }
        let value = scalar.value
        return value >= 0x20 && value != 0x7F
    }
}

