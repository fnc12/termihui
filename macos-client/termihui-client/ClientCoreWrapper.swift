import Foundation

/// Swift обёртка для C++ ядра клиента TermiHUI
class ClientCoreWrapper {
    
    /// Инициализирует и запускает ядро клиента TermiHUI
    /// @return true если успешно запущено, false в случае ошибки
    static func initializeApp() -> Bool {
        print("🔗 ClientCoreWrapper: Инициализация C++ ядра...")
        let result = termihui_create_app()
        
        if result {
            print("✅ ClientCoreWrapper: C++ ядро успешно инициализировано")
        } else {
            print("❌ ClientCoreWrapper: Ошибка инициализации C++ ядра")
        }
        
        return result
    }
}
