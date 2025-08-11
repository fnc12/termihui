#include "CompletionManager.h"

#include <dirent.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <cstdlib>
#include <iostream>

CompletionManager::CompletionManager()
{
    // Инициализируем список популярных команд
    commonCommands = {
        "ls", "cd", "pwd", "cat", "grep", "find", "mkdir", "rm", "cp", "mv",
        "chmod", "chown", "ps", "kill", "top", "df", "du", "free", "uname",
        "whoami", "which", "whereis", "locate", "man", "history", "clear",
        "echo", "touch", "head", "tail", "less", "more", "wc", "sort", "uniq",
        "tar", "gzip", "gunzip", "zip", "unzip", "wget", "curl", "ssh", "scp"
    };
}

std::vector<std::string> CompletionManager::getCompletions(const std::string& text, int cursorPosition, const std::string& currentDir) const
{
    std::vector<std::string> completions;
    
    // Простая логика автодополнения
    if (text.empty()) {
        // Если текст пустой, предлагаем популярные команды
        completions = {"ls", "cd", "pwd", "cat", "grep", "find", "mkdir", "rm", "cp", "mv"};
    } else {
        // Анализируем текст для определения типа автодополнения
        std::string lastWord = extractLastWord(text, cursorPosition);
        
        if (isCommand(text, cursorPosition)) {
            // Автодополнение команд
            completions = getCommandCompletions(lastWord);
        } else {
            // Автодополнение файлов и директорий
            completions = getFileCompletions(lastWord, currentDir);
        }
    }
    
    std::cout << "CompletionManager: Найдено " << completions.size() << " вариантов для '" << text << "' в директории '" << currentDir << "'" << std::endl;
    return completions;
}

std::string CompletionManager::extractLastWord(const std::string& text, int cursorPosition) const
{
    if (text.empty() || cursorPosition <= 0) {
        return "";
    }
    
    // Находим начало последнего слова (ищем пробел справа налево)
    int start = cursorPosition - 1;
    while (start >= 0 && text[start] != ' ' && text[start] != '\t') {
        start--;
    }
    start++; // Переходим к началу слова
    
    return text.substr(start, cursorPosition - start);
}

bool CompletionManager::isCommand(const std::string& text, int cursorPosition) const
{
    // Простая эвристика: если курсор в начале строки или после пробела/таба,
    // то это команда, иначе - аргумент/файл
    if (cursorPosition == 0) {
        return true;
    }
    
    // Ищем пробелы до позиции курсора
    for (int i = 0; i < cursorPosition && i < static_cast<int>(text.length()); i++) {
        if (text[i] == ' ' || text[i] == '\t') {
            return false; // Нашли пробел, значит это не команда
        }
    }
    
    return true; // Пробелов не найдено, это команда
}

std::vector<std::string> CompletionManager::getCommandCompletions(const std::string& prefix) const
{
    std::vector<std::string> matches;
    
    for (const auto& cmd : commonCommands) {
        if (cmd.substr(0, prefix.length()) == prefix) {
            matches.push_back(cmd);
        }
    }
    
    return matches;
}

std::vector<std::string> CompletionManager::getFileCompletions(const std::string& prefix, const std::string& currentDir) const
{
    std::vector<std::string> matches;
    
    // Определяем директорию для поиска
    std::string searchDir = currentDir;
    std::string filePrefix = prefix;
    std::string originalDirPrefix = "";
    
    // Если в префиксе есть слеш, разделяем путь и имя файла
    size_t lastSlash = prefix.find_last_of('/');
    if (lastSlash != std::string::npos) {
        searchDir = prefix.substr(0, lastSlash);
        filePrefix = prefix.substr(lastSlash + 1);
        originalDirPrefix = searchDir + "/"; // Сохраняем оригинальный префикс с тильдой
        
        if (searchDir.empty()) {
            searchDir = "/"; // Корневая директория
            originalDirPrefix = "/";
        }
    } else {
        // Если нет слеша, ищем в текущей директории
        searchDir = currentDir;
    }
    
    // Раскрываем тильду в домашнюю директорию для поиска
    std::string expandedSearchDir = expandTilde(searchDir);
    
    std::cout << "CompletionManager: Поиск файлов в '" << searchDir << "' (раскрыто: '" 
              << expandedSearchDir << "'), префикс: '" << filePrefix << "'" << std::endl;
    
    // Открываем директорию
    DIR* dir = opendir(expandedSearchDir.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string filename = entry->d_name;
            
            // Пропускаем скрытые файлы (начинающиеся с точки)
            if (filename[0] == '.') {
                continue;
            }
            
            // Проверяем совпадение с префиксом
            if (filename.substr(0, filePrefix.length()) == filePrefix) {
                // Если это директория, добавляем слеш в конце
                std::string fullPath = expandedSearchDir + "/" + filename;
                struct stat statbuf;
                if (stat(fullPath.c_str(), &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                    filename += "/";
                }
                
                // Возвращаем полный путь с оригинальным префиксом (включая тильду)
                matches.push_back(originalDirPrefix + filename);
            }
        }
        closedir(dir);
        
        std::cout << "CompletionManager: Найдено " << matches.size() << " файлов" << std::endl;
    } else {
        std::cout << "CompletionManager: Не удалось открыть директорию '" << expandedSearchDir << "'" << std::endl;
    }
    
    return matches;
}

std::string CompletionManager::expandTilde(const std::string& path) const
{
    if (path.empty() || path[0] != '~') {
        return path; // Нет тильды, возвращаем как есть
    }
    
    if (path.length() == 1 || path[1] == '/') {
        // ~/... - домашняя директория текущего пользователя
        const char* homeDir = getenv("HOME");
        if (!homeDir) {
            // Если переменная HOME не установлена, получаем из passwd
            struct passwd* pw = getpwuid(getuid());
            if (pw) {
                homeDir = pw->pw_dir;
            }
        }
        
        if (homeDir) {
            if (path.length() == 1) {
                return std::string(homeDir); // Просто ~
            } else {
                return std::string(homeDir) + path.substr(1); // ~/path
            }
        }
    } else {
        // ~username/... - домашняя директория конкретного пользователя
        size_t slashPos = path.find('/');
        std::string username = path.substr(1, slashPos - 1);
        
        struct passwd* pw = getpwnam(username.c_str());
        if (pw) {
            if (slashPos == std::string::npos) {
                return std::string(pw->pw_dir); // Просто ~username
            } else {
                return std::string(pw->pw_dir) + path.substr(slashPos); // ~username/path
            }
        }
    }
    
    // Если не удалось раскрыть тильду, возвращаем исходный путь
    return path;
}
