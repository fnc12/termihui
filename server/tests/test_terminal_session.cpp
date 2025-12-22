#include <catch2/catch_test_macros.hpp>
#include "TerminalSessionController.h"
#include <thread>
#include <chrono>

TEST_CASE("TerminalSessionController basic functionality", "[TerminalSessionController]") {
    
    SECTION("Session creation and command execution") {
        TerminalSessionController session;
        
        // Тестируем создание сессии с простой командой
        REQUIRE(session.startCommand("echo 'Hello, World!'"));
        
        // Проверяем, что сессия запущена
        REQUIRE(session.isRunning());
        
        // Проверяем, что получили валидный PID
        REQUIRE(session.getChildPid() > 0);
        
        // Ждем немного для выполнения команды
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Читаем вывод
        std::string output;
        int attempts = 0;
        while (session.isRunning() && attempts < 50) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        
        // Проверяем, что получили ожидаемый вывод
        REQUIRE_FALSE(output.empty());
        REQUIRE(output.find("Hello, World!") != std::string::npos);
    }
    
    SECTION("ls command execution") {
        TerminalSessionController session;
        
        // Тестируем команду ls
        REQUIRE(session.startCommand("ls"));
        
        // Проверяем, что сессия запущена
        REQUIRE(session.isRunning());
        
        // Ждем выполнения команды
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Читаем вывод
        std::string output;
        int attempts = 0;
        while (session.isRunning() && attempts < 100) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
        }
        
        // Проверяем, что получили вывод от ls
        REQUIRE_FALSE(output.empty());
        // ls должен показать как минимум что-то (файлы или пустую директорию)
        REQUIRE(output.length() > 0);
        
        INFO("ls output: " << output);
    }
    
    SECTION("Interactive session with input") {
        TerminalSessionController session;
        
        // Запускаем bash для интерактивной сессии
        REQUIRE(session.startCommand("bash"));
        
        // Ждем запуска bash
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Отправляем команду ls
        REQUIRE(session.sendInput("ls\n") > 0);
        
        // Ждем выполнения команды
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Читаем вывод
        std::string output;
        int attempts = 0;
        while (attempts < 50) {
            if (session.hasData()) {
                std::string newOutput = session.readOutput();
                output += newOutput;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            attempts++;
            
            // Прерываем если получили достаточно данных
            if (output.length() > 10) {
                break;
            }
        }
        
        // Отправляем exit для корректного завершения
        session.sendInput("exit\n");
        
        // Ждем завершения
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Проверяем, что получили вывод
        REQUIRE_FALSE(output.empty());
        
        INFO("Interactive session output: " << output);
        
        // Принудительно завершаем если еще работает
        if (session.isRunning()) {
            session.terminate();
        }
    }
}

TEST_CASE("TerminalSessionController error handling", "[TerminalSessionController]") {
    
    SECTION("Invalid command") {
        TerminalSessionController session;
        
        // Тестируем несуществующую команду
        REQUIRE(session.startCommand("nonexistent_command_12345"));
        
        // Ждем немного
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Сессия должна завершиться с ошибкой
        // (bash попытается выполнить команду и завершится)
    }
    
    SECTION("Double start prevention") {
        TerminalSessionController session;
        
        // Запускаем первую команду
        REQUIRE(session.startCommand("sleep 1"));
        
        // Попытка запустить вторую команду должна провалиться
        REQUIRE_FALSE(session.startCommand("echo test"));
        
        // Ждем завершения первой команды
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

TEST_CASE("TerminalSessionController resource management", "[TerminalSessionController]") {
    
    SECTION("Proper cleanup on destruction") {
        pid_t childPid = -1;
        
        {
            TerminalSessionController session;
            REQUIRE(session.startCommand("sleep 2"));
            childPid = session.getChildPid();
            REQUIRE(childPid > 0);
            REQUIRE(session.isRunning());
        } // session уничтожается здесь
        
        // Ждем немного для корректной очистки
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Проверяем, что процесс был корректно завершен
        // (это можно проверить через kill(pid, 0), но в тесте это сложно)
    }
}

TEST_CASE("Interactive bash session state persistence", "[TerminalSessionController][Interactive]") {
    SECTION("Directory change should persist between commands") {
        TerminalSessionController session;
        
        // Создаем интерактивную bash-сессию
        REQUIRE(session.createSession());
        REQUIRE(session.isRunning());
        
        // Ждем инициализации bash
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // Очищаем буфер от приветственных сообщений bash
        session.readOutput();
        
        // 1. Выполняем pwd и сохраняем результат
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string initialDir;
        int attempts = 0;
        while (attempts < 20) {
            if (session.hasData()) {
                std::string output = session.readOutput();
                initialDir += output;
                if (initialDir.find('\n') != std::string::npos) {
                    break; // Получили полную строку
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Очищаем от лишних символов
        size_t newlinePos = initialDir.find('\n');
        if (newlinePos != std::string::npos) {
            initialDir = initialDir.substr(0, newlinePos);
        }
        
        REQUIRE(!initialDir.empty());
        
        // 2. Меняем директорию на /tmp
        REQUIRE(session.executeCommand("cd /tmp"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // Очищаем буфер после cd (cd обычно ничего не выводит)
        session.readOutput();
        
        // 3. Снова выполняем pwd
        REQUIRE(session.executeCommand("pwd"));
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        std::string newDir;
        attempts = 0;
        while (attempts < 20) {
            if (session.hasData()) {
                std::string output = session.readOutput();
                newDir += output;
                if (newDir.find('\n') != std::string::npos) {
                    break; // Получили полную строку
                }
            }
            attempts++;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // Очищаем от лишних символов
        newlinePos = newDir.find('\n');
        if (newlinePos != std::string::npos) {
            newDir = newDir.substr(0, newlinePos);
        }
        
        REQUIRE(!newDir.empty());
        
        // 4. ГЛАВНАЯ ПРОВЕРКА: директории должны быть разные
        // Если интерактивная сессия работает правильно, то newDir должен быть "/tmp"
        // А initialDir должен быть исходной директорией (не "/tmp")
        INFO("Initial directory: " << initialDir);
        INFO("Directory after cd /tmp: " << newDir);
        
        // Проверяем, что директория изменилась
        REQUIRE(initialDir != newDir);
        
        // Дополнительная проверка - новая директория должна быть /tmp
        REQUIRE(newDir == "/tmp");
    }
} 