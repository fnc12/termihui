cmake_minimum_required(VERSION 3.16)
project(termihui VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent для всех зависимостей
include(FetchContent)

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

message(STATUS "=== All dependencies ready ===")
message(STATUS "")

# ============================================
# Подпроекты
# ============================================

# Client Core (C++ библиотека для клиентов)
add_subdirectory(client-core)

# Server
add_subdirectory(server)

