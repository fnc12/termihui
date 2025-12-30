cmake_minimum_required(VERSION 3.15)
project(termihui-server VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Если собираемся standalone (не как подпроект) — качаем зависимости сами
if(NOT TARGET fmt::fmt OR NOT TARGET hv_static OR NOT TARGET Catch2::Catch2WithMain)
include(FetchContent)

    if(NOT TARGET Catch2::Catch2WithMain)
        message(STATUS "Fetching Catch2 (standalone build)...")
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)
    endif()

    if(NOT TARGET hv_static)
        message(STATUS "Fetching libhv (standalone build)...")
FetchContent_Declare(
    libhv
    GIT_REPOSITORY https://github.com/ithewei/libhv.git
    GIT_TAG v1.3.2
)
FetchContent_MakeAvailable(libhv)
    endif()

    if(NOT TARGET fmt::fmt)
        message(STATUS "Fetching fmt (standalone build)...")
FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.1.1
)
FetchContent_MakeAvailable(fmt)
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
    endif()
endif()

# For macOS/Linux check for required libraries
if(NOT WIN32)
    find_package(Threads REQUIRED)
    include(CheckLibraryExists)
    check_library_exists(util forkpty "" HAVE_FORKPTY)
    if(HAVE_FORKPTY)
        set(UTIL_LIBRARIES util)
    endif()
endif()

# Main project sources
set(SOURCES
    src/TerminalSessionController.cpp
    src/WebSocketServer.cpp
    src/CompletionManager.cpp
    src/JsonHelper.cpp
    src/TermihuiServerController.cpp
    src/FileSystemManager.cpp
    src/ServerStorage.cpp
    src/SessionStorage.cpp
    src/main.cpp
)

set(HEADERS
    src/TerminalSessionController.h
    src/WebSocketServer.h
    src/CompletionManager.h
    src/JsonHelper.h
    src/TermihuiServerController.h
    src/FileSystemManager.h
    src/ServerStorage.h
    src/ServerStorageModels.h
    src/SessionStorage.h
    src/SessionStorageModels.h
    src/SessionStorageSchema.h
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Compiler settings
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-gnu-anonymous-struct      # Suppress libhv warnings
    -Wno-nested-anon-types         # Suppress libhv warnings
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
        platform_folders
        sqlite_orm::sqlite_orm
    )
endif()

# Include directories for headers
target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${platform_folders_SOURCE_DIR}
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
    tests/test_termihui_server_controller.cpp
    src/TerminalSessionController.cpp
    src/CompletionManager.cpp
    src/TermihuiServerController.cpp
    src/WebSocketServer.cpp
    src/JsonHelper.cpp
    src/FileSystemManager.cpp
    src/ServerStorage.cpp
    src/SessionStorage.cpp
)

add_executable(unit_tests ${TEST_SOURCES})

# C++20 for tests (needed for default comparison operators)
set_target_properties(unit_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

# Compiler settings for tests
target_compile_options(unit_tests PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wno-gnu-anonymous-struct      # Suppress libhv warnings
    -Wno-nested-anon-types         # Suppress libhv warnings
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
        hv_static
        platform_folders
        sqlite_orm::sqlite_orm
    )
endif()

# Include directories for headers for tests
target_include_directories(unit_tests PRIVATE
    src
    tests
    ${platform_folders_SOURCE_DIR}
)

# Enable CTest support
enable_testing()
add_test(NAME unit_tests COMMAND unit_tests) 
