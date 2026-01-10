cmake_minimum_required(VERSION 3.16)

# Enable Objective-C++ on Apple platforms
if(APPLE)
    project(termihui_client_core VERSION 1.0.0 LANGUAGES CXX OBJCXX)
else()
    project(termihui_client_core VERSION 1.0.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add shared library (with guard for when building as part of parent project)
if(NOT TARGET termihui_shared)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_CURRENT_BINARY_DIR}/shared)
endif()

# Если собираемся standalone (не как подпроект) — качаем зависимости сами
include(FetchContent)

if(NOT TARGET fmt::fmt)
    message(STATUS "Fetching fmt (standalone build)...")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        10.1.1
)
FetchContent_MakeAvailable(fmt)
    message(STATUS "fmt ready")
endif()

if(NOT TARGET hv_static)
    message(STATUS "Fetching libhv (standalone build)...")
    FetchContent_Declare(
        libhv
        GIT_REPOSITORY https://github.com/ithewei/libhv.git
        GIT_TAG        v1.3.2
    )
    FetchContent_MakeAvailable(libhv)
    message(STATUS "libhv ready")
endif()

if(NOT TARGET sqlite_orm::sqlite_orm)
    message(STATUS "Fetching sqlite_orm (standalone build)...")
    FetchContent_Declare(
        sqlite_orm
        GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
        GIT_TAG v1.9.1
    )
    FetchContent_MakeAvailable(sqlite_orm)
    add_compile_definitions(SQLITE_ORM_WITH_CPP_20)
    add_compile_definitions(SQLITE_ORM_OMITS_CODECVT)
    message(STATUS "sqlite_orm ready")
endif()

# Platform-specific clipboard sources
if(APPLE)
    set(CLIPBOARD_SOURCES src/clipboard/clipboard_manager_macos.mm)
elseif(WIN32)
    set(CLIPBOARD_SOURCES src/clipboard/clipboard_manager_windows.cpp)
elseif(ANDROID)
    set(CLIPBOARD_SOURCES src/clipboard/clipboard_manager_android.cpp)
else()
    set(CLIPBOARD_SOURCES src/clipboard/clipboard_manager_linux.cpp)
endif()

# Создаём статическую библиотеку
add_library(termihui_client_core STATIC
    src/client_core.cpp
    src/ansi_parser.cpp
    src/websocket_client_controller.cpp
    src/client_storage.cpp
    ${CLIPBOARD_SOURCES}
)

# Публичные заголовочные файлы
target_include_directories(termihui_client_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Линкуем зависимости
target_link_libraries(termihui_client_core
    PRIVATE
        fmt::fmt
        hv_static
        sqlite_orm::sqlite_orm
    PUBLIC
        termihui_shared
)

# Platform-specific frameworks
if(APPLE)
    target_link_libraries(termihui_client_core PRIVATE "-framework AppKit")
endif()

# Настройки выходного файла
set_target_properties(termihui_client_core PROPERTIES
    OUTPUT_NAME "termihui_client_core"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib"
)

# Опциональная установка
install(TARGETS termihui_client_core
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)

# ============================================
# Unit tests
# ============================================

if(TARGET Catch2::Catch2WithMain)
    set(TEST_SOURCES
        tests/test_main.cpp
        tests/test_client_core_controller.cpp
        tests/ClientCoreControllerTestable.cpp
    )

    add_executable(client_core_tests ${TEST_SOURCES})

    # C++20 for tests (needed for default comparison operators)
    set_target_properties(client_core_tests PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON
    )

    target_link_libraries(client_core_tests PRIVATE
        termihui_client_core
        Catch2::Catch2WithMain
        fmt::fmt
        hv_static
    )

    target_include_directories(client_core_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    enable_testing()
    add_test(NAME client_core_tests COMMAND client_core_tests)
endif()
