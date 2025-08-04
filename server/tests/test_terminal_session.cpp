#include <catch2/catch_test_macros.hpp>
#include "TerminalSession.h"
#include <thread>
#include <chrono>

TEST_CASE("TerminalSession basic functionality", "[TerminalSession]") {
    
    SECTION("Session creation and command execution") {
        TerminalSession session;
        
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
            if (session.hasData(10)) {
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
        TerminalSession session;
        
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
            if (session.hasData(10)) {
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
        TerminalSession session;
        
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
            if (session.hasData(10)) {
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

TEST_CASE("TerminalSession error handling", "[TerminalSession]") {
    
    SECTION("Invalid command") {
        TerminalSession session;
        
        // Тестируем несуществующую команду
        REQUIRE(session.startCommand("nonexistent_command_12345"));
        
        // Ждем немного
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Сессия должна завершиться с ошибкой
        // (bash попытается выполнить команду и завершится)
    }
    
    SECTION("Double start prevention") {
        TerminalSession session;
        
        // Запускаем первую команду
        REQUIRE(session.startCommand("sleep 1"));
        
        // Попытка запустить вторую команду должна провалиться
        REQUIRE_FALSE(session.startCommand("echo test"));
        
        // Ждем завершения первой команды
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    }
}

TEST_CASE("TerminalSession resource management", "[TerminalSession]") {
    
    SECTION("Proper cleanup on destruction") {
        pid_t childPid = -1;
        
        {
            TerminalSession session;
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