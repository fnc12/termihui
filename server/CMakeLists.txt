cmake_minimum_required(VERSION 3.15)
project(termihui-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Включаем FetchContent для загрузки зависимостей
include(FetchContent)

# Подключаем Catch2 для юнит-тестов
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

# Подключаем libhv для WebSocket сервера
FetchContent_Declare(
    libhv
    GIT_REPOSITORY https://github.com/ithewei/libhv.git
    GIT_TAG v1.3.2
)

FetchContent_MakeAvailable(libhv)

# Подключаем fmt для форматированного вывода
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.1.1
)

FetchContent_MakeAvailable(fmt)

# Для macOS/Linux проверяем наличие необходимых библиотек
if(NOT WIN32)
    find_package(Threads REQUIRED)
    # Проверяем доступность util library для forkpty
    include(CheckLibraryExists)
    check_library_exists(util forkpty "" HAVE_FORKPTY)
    if(HAVE_FORKPTY)
        set(UTIL_LIBRARIES util)
    endif()
endif()

# Исходники основного проекта
set(SOURCES
    src/TerminalSession.cpp
    src/WebSocketServer.cpp
    src/main.cpp
)

set(HEADERS
    src/TerminalSession.h
    src/WebSocketServer.h
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Настройки компилятора
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Линковка библиотек
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        ${UTIL_LIBRARIES}
        hv_static
        fmt::fmt
    )
endif()

# Включаем директории для заголовочных файлов
target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

# Для Debug режима добавляем больше отладочной информации
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

# Создаем исполняемый файл для юнит-тестов
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_interactive_session.cpp
    src/TerminalSession.cpp
)

add_executable(unit_tests ${TEST_SOURCES})

# Настройки компилятора для тестов
target_compile_options(unit_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Линковка библиотек для тестов
if(NOT WIN32)
    target_link_libraries(unit_tests PRIVATE
        Catch2::Catch2WithMain
        Threads::Threads
        ${UTIL_LIBRARIES}
        fmt::fmt
    )
endif()

# Включаем директории для заголовочных файлов для тестов
target_include_directories(unit_tests PRIVATE
    src
    tests
)

# Включаем поддержку CTest
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests) 