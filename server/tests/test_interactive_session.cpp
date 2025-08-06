#include <catch2/catch_test_macros.hpp>
#include "TerminalSession.h"
#include <thread>
#include <chrono>

TEST_CASE("Interactive bash session state persistence", "[TerminalSession][Interactive]") {
    SECTION("Directory change should persist between commands") {
        TerminalSession session;
        
        // Создаем интерактивную bash-сессию
        REQUIRE(session.createSession());
        REQUIRE(session.isRunning());
        
        // Ждем инициализации bash
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Очищаем буфер от приветственных сообщений bash
        session.readOutput();
        
        // 1. Выполняем pwd и сохраняем результат
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Увеличиваем время ожидания
        
        std::string initialDir;
        std::string allOutput; // Для отладки - сохраним весь вывод
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                std::string output = session.readOutput();
                allOutput += output; // Сохраняем для отладки
                initialDir += output;
                if (initialDir.find('\n') != std::string::npos) {
                    break; // Получили полную строку
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from first pwd: '" << allOutput << "'");
        
        REQUIRE(!allOutput.empty());
        
        // 2. Меняем директорию на /tmp
        REQUIRE(session.executeCommand("cd /tmp"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Очищаем буфер после cd (cd обычно ничего не выводит)
        session.readOutput();
        
        // 3. Снова выполняем pwd
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        std::string newOutput; // Весь вывод второй команды pwd
        attempts = 0;
        while (attempts < 20) {
            if (session.hasData(50)) {
                std::string output = session.readOutput();
                newOutput += output;
                if (newOutput.find("bash-3.2$") != std::string::npos) {
                    break; // Получили полный вывод с промптом
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        INFO("Output from second pwd: '" << newOutput << "'");
        
        // 4. ГЛАВНАЯ ПРОВЕРКА: если интерактивная сессия работает правильно,
        // то вывод команд pwd должен быть РАЗНЫЙ (разные директории)
        REQUIRE(!newOutput.empty());
        
        // Если cd сработал, то выводы должны отличаться
        REQUIRE(allOutput != newOutput);
    }
}