cmake_minimum_required(VERSION 3.16)
project(termihui_client_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(NOT TARGET platform_folders)
    message(STATUS "Fetching platform_folders (standalone build)...")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
    FetchContent_Declare(
        platform_folders
        GIT_REPOSITORY https://github.com/sago007/PlatformFolders.git
        GIT_TAG 4.2.0
    )
    FetchContent_MakeAvailable(platform_folders)
    message(STATUS "platform_folders ready")
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
    message(STATUS "sqlite_orm ready")
endif()

# Создаём статическую библиотеку
add_library(termihui_client_core STATIC
    src/client_core.cpp
    src/ansi_parser.cpp
    src/websocket_client_controller.cpp
    src/client_storage.cpp
    ${platform_folders_SOURCE_DIR}/sago/platform_folders.cpp
)

# Публичные заголовочные файлы
target_include_directories(termihui_client_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${platform_folders_SOURCE_DIR}
)

# Линкуем зависимости
target_link_libraries(termihui_client_core
    PRIVATE
        fmt::fmt
        hv_static
        platform_folders
        sqlite_orm::sqlite_orm
)

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
