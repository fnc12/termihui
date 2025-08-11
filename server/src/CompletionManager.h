#pragma once

#include <string>
#include <vector>

/**
 * Менеджер автодополнения для терминальных команд и файлов
 * 
 * Функциональность:
 * - Автодополнение команд из списка популярных команд
 * - Автодополнение файлов и директорий
 * - Поддержка тильды (~) для домашних директорий
 * - Определение контекста (команда vs файл)
 */
class CompletionManager {
public:
    /**
     * Конструктор
     */
    CompletionManager();
    
    /**
     * Деструктор
     */
    ~CompletionManager() = default;
    
    // Запрещаем копирование
    CompletionManager(const CompletionManager&) = delete;
    CompletionManager& operator=(const CompletionManager&) = delete;
    
    /**
     * Получение вариантов автодополнения для текста
     * @param text полный текст команды
     * @param cursorPosition позиция курсора в тексте
     * @param currentDir текущая рабочая директория для поиска файлов
     * @return вектор строк с вариантами автодополнения
     */
    std::vector<std::string> getCompletions(const std::string& text, int cursorPosition, const std::string& currentDir = ".") const;
    
private:
    /**
     * Извлечение последнего слова из текста
     * @param text текст для анализа
     * @param cursorPosition позиция курсора
     * @return последнее слово до курсора
     */
    std::string extractLastWord(const std::string& text, int cursorPosition) const;
    
    /**
     * Проверка, является ли текущая позиция командой
     * @param text полный текст
     * @param cursorPosition позиция курсора
     * @return true если это команда, false если аргумент/файл
     */
    bool isCommand(const std::string& text, int cursorPosition) const;
    
    /**
     * Получение автодополнения для команд
     * @param prefix префикс команды
     * @return список подходящих команд
     */
    std::vector<std::string> getCommandCompletions(const std::string& prefix) const;
    
    /**
     * Получение автодополнения для файлов и директорий
     * @param prefix префикс пути
     * @param currentDir текущая рабочая директория
     * @return список подходящих файлов/директорий
     */
    std::vector<std::string> getFileCompletions(const std::string& prefix, const std::string& currentDir = ".") const;
    
    /**
     * Раскрытие тильды (~) в полный путь к домашней директории
     * @param path путь с возможной тильдой
     * @return путь с раскрытой тильдой
     */
    std::string expandTilde(const std::string& path) const;
    
private:
    // Список популярных команд для автодополнения
    std::vector<std::string> commonCommands;
};
