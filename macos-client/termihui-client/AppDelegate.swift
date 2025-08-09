import Cocoa

@main
class AppDelegate: NSObject, NSApplicationDelegate {

    


    func applicationDidFinishLaunching(_ aNotification: Notification) {
        print("🚀 AppDelegate: Запуск приложения TermiHUI")
        
        // Инициализируем C++ ядро клиента
        let coreInitialized = ClientCoreWrapper.initializeApp()
        
        if !coreInitialized {
            print("❌ AppDelegate: Критическая ошибка - не удалось инициализировать ядро")
            NSApplication.shared.terminate(self)
            return
        }
        
        print("✅ AppDelegate: Приложение успешно запущено")
    }

    func applicationWillTerminate(_ aNotification: Notification) {
        // Insert code here to tear down your application
    }

    func applicationSupportsSecureRestorableState(_ app: NSApplication) -> Bool {
        return true
    }


}

