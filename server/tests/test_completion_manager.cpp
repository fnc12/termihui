#include <catch2/catch_test_macros.hpp>
#include "CompletionManager.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstdlib>

// Вспомогательные функции для тестов
bool contains(const std::vector<std::string>& container, const std::string& item) {
    for (const auto& elem : container) {
        if (elem == item) {
            return true;
        }
    }
    return false;
}

TEST_CASE("CompletionManager command completion", "[completion]") {
    CompletionManager manager;
    
    SECTION("Complete 'ls' command") {
        auto completions = manager.getCompletions("ls", 2);
        REQUIRE(contains(completions, "ls"));
    }
    
    SECTION("Complete 'pw' to 'pwd'") {
        auto completions = manager.getCompletions("pw", 2);
        REQUIRE(contains(completions, "pwd"));
    }
}

TEST_CASE("CompletionManager empty input", "[completion]") {
    CompletionManager manager;
    
    auto completions = manager.getCompletions("", 0);
    REQUIRE(!completions.empty());
    REQUIRE(contains(completions, "ls"));
}

TEST_CASE("CompletionManager file completion", "[completion]") {
    CompletionManager manager;
    
    SECTION("File completion in current directory") {
        auto completions = manager.getCompletions("cat tes", 7);
        // Результат зависит от содержимого директории, просто проверяем что не упало
        INFO("Found " << completions.size() << " file completions for 'tes'");
        REQUIRE(true); // Тест проходит если не упал
    }
}

TEST_CASE("CompletionManager tilde expansion", "[completion][tilde]") {
    CompletionManager manager;
    
    // Получаем домашнюю директорию
    const char* home = getenv("HOME");
    if (!home) {
        SKIP("HOME environment variable not set");
    }
    
    SECTION("Tilde expansion for ~/De pattern") {
        auto completions = manager.getCompletions("cd ~/De", 7);
        
        INFO("Testing tilde expansion for 'cd ~/De'");
        INFO("HOME directory: " << home);
        INFO("Found " << completions.size() << " completions");
        
        // Выводим найденные варианты для отладки
        for (const auto& completion : completions) {
            INFO("  - " << completion);
            
            // Проверяем что результат начинается с тильды (сохраняет оригинальный формат)
            if (!completion.empty() && completion[0] == '~') {
                INFO("  ✅ Correctly preserved tilde format");
            }
        }
        
        // Проверяем что есть хотя бы один результат с тильдой
        bool has_tilde_result = false;
        for (const auto& completion : completions) {
            if (!completion.empty() && completion[0] == '~') {
                has_tilde_result = true;
                break;
            }
        }
        
        // Если есть результаты, они должны сохранять формат с тильдой
        if (!completions.empty()) {
            REQUIRE(has_tilde_result);
        }
    }
}

TEST_CASE("CompletionManager path parsing", "[completion]") {
    CompletionManager manager;
    
    // Тест различных форматов путей
    std::vector<std::pair<std::string, int>> test_cases = {
        {"ls", 2},                    // Команда
        {"ls file", 7},               // Файл
        {"cd /usr/", 8},              // Абсолютный путь
        {"cat ~/Documents/", 15},     // Тильда с подпутем
        {"vim ./src/", 9}             // Относительный путь
    };
    
    for (const auto& test_case : test_cases) {
        DYNAMIC_SECTION("Testing path: " << test_case.first << " at position " << test_case.second) {
            auto completions = manager.getCompletions(test_case.first, test_case.second);
            INFO("Found " << completions.size() << " completions");
            
            // Выводим первые несколько результатов для отладки
            int count = 0;
            for (const auto& completion : completions) {
                if (count++ >= 3) break;
                INFO("  - " << completion);
            }
            
            // Тест проходит если не упал
            REQUIRE(true);
        }
    }
}
