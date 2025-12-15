cmake_minimum_required(VERSION 3.15)
project(termihui-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include FetchContent for dependency downloading
include(FetchContent)

# Fetch Catch2 for unit tests
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)

FetchContent_MakeAvailable(Catch2)

# Fetch libhv for WebSocket server
FetchContent_Declare(
    libhv
    GIT_REPOSITORY https://github.com/ithewei/libhv.git
    GIT_TAG v1.3.2
)

FetchContent_MakeAvailable(libhv)

# Fetch fmt for formatted output
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.1.1
)

FetchContent_MakeAvailable(fmt)

# For macOS/Linux check for required libraries
if(NOT WIN32)
    find_package(Threads REQUIRED)
    # Check util library availability for forkpty
    include(CheckLibraryExists)
    check_library_exists(util forkpty "" HAVE_FORKPTY)
    if(HAVE_FORKPTY)
        set(UTIL_LIBRARIES util)
    endif()
endif()

# Main project sources
set(SOURCES
    src/TerminalSession.cpp
    src/WebSocketServer.cpp
    src/CompletionManager.cpp
    src/JsonHelper.cpp
    src/TermihuiServer.cpp
    src/main.cpp
)

set(HEADERS
    src/TerminalSession.h
    src/WebSocketServer.h
    src/CompletionManager.h
    src/JsonHelper.h
    src/TermihuiServer.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Compiler settings
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Link libraries
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Threads::Threads
        ${UTIL_LIBRARIES}
        hv_static
        fmt::fmt
    )
endif()

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE
    src
)

# For Debug mode add more debug information
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
endif()

# Create executable for unit tests
set(TEST_SOURCES
    tests/test_main.cpp
    tests/test_interactive_session.cpp
    tests/test_completion_manager.cpp
    src/TerminalSession.cpp
    src/CompletionManager.cpp
)

add_executable(unit_tests ${TEST_SOURCES})

# Compiler settings for tests
target_compile_options(unit_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-g>
    $<$<CONFIG:Release>:-O3>
)

# Link libraries for tests
if(NOT WIN32)
    target_link_libraries(unit_tests PRIVATE
        Catch2::Catch2WithMain
        Threads::Threads
        ${UTIL_LIBRARIES}
        fmt::fmt
    )
endif()

# Include directories for headers for tests
target_include_directories(unit_tests PRIVATE
    src
    tests
)

# Enable CTest support
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests) 