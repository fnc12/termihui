cmake_minimum_required(VERSION 3.16)
project(termihui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent для всех зависимостей
include(FetchContent)

# Хранить зависимости вне build директории (не удаляются при rm -rf build/*)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/.deps" CACHE PATH "Dependencies cache")

# ============================================
# Общие зависимости (скачиваются один раз)
# ============================================

message(STATUS "")
message(STATUS "=== Fetching dependencies ===")

# fmt
message(STATUS "Fetching fmt...")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        10.1.1
)
FetchContent_MakeAvailable(fmt)
message(STATUS "fmt ready")

# libhv (для сервера)
message(STATUS "Fetching libhv...")
FetchContent_Declare(
    libhv
    GIT_REPOSITORY https://github.com/ithewei/libhv.git
    GIT_TAG        v1.3.2
)
FetchContent_MakeAvailable(libhv)
message(STATUS "libhv ready")

# Catch2 (для тестов)
message(STATUS "Fetching Catch2...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(Catch2)
message(STATUS "Catch2 ready")

# sqlite_orm (для сервера)
message(STATUS "Fetching sqlite_orm...")
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
FetchContent_Declare(
    sqlite_orm
    GIT_REPOSITORY https://github.com/fnc12/sqlite_orm.git
    GIT_TAG        v1.9.1
)
add_compile_definitions(SQLITE_ORM_WITH_CPP_20)
FetchContent_MakeAvailable(sqlite_orm)
message(STATUS "sqlite_orm ready")

# platform_folders (для системных путей)
message(STATUS "Fetching platform_folders...")
FetchContent_Declare(
    platform_folders
    GIT_REPOSITORY https://github.com/sago007/PlatformFolders.git
    GIT_TAG        4.2.0
)
FetchContent_MakeAvailable(platform_folders)
message(STATUS "platform_folders ready")

message(STATUS "=== All dependencies ready ===")
message(STATUS "")

# ============================================
# Подпроекты
# ============================================

# Shared library (protocol messages)
add_subdirectory(shared)

# Client Core (C++ библиотека для клиентов)
add_subdirectory(client-core)

# Server
add_subdirectory(server)

