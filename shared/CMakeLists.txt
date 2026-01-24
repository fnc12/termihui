cmake_minimum_required(VERSION 3.16)

# Enable Objective-C++ for Apple platforms
if(APPLE)
    project(termihui_shared VERSION 1.0.0 LANGUAGES CXX OBJCXX)
else()
    project(termihui_shared VERSION 1.0.0 LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch libhv if not available (for nlohmann::json)
if(NOT TARGET hv_static)
    include(FetchContent)
    message(STATUS "Fetching libhv for termihui_shared...")
    FetchContent_Declare(
        libhv
        GIT_REPOSITORY https://github.com/ithewei/libhv.git
        GIT_TAG v1.3.2
    )
    FetchContent_MakeAvailable(libhv)
endif()

# Platform-specific FileSystemManager implementation
if(APPLE)
    set(FILESYSTEM_SOURCES src/filesystem/file_system_manager_apple.mm)
else()
    # Desktop (Windows/Linux) - needs platform_folders
    if(NOT TARGET platform_folders)
        include(FetchContent)
        message(STATUS "Fetching platform_folders for termihui_shared...")
        set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "")
        FetchContent_Declare(
            platform_folders
            GIT_REPOSITORY https://github.com/sago007/PlatformFolders.git
            GIT_TAG 4.2.0
        )
        FetchContent_MakeAvailable(platform_folders)
    endif()
    set(FILESYSTEM_SOURCES 
        src/filesystem/file_system_manager_desktop.cpp
        ${platform_folders_SOURCE_DIR}/sago/platform_folders.cpp
    )
endif()

# Static library
add_library(termihui_shared STATIC
    src/json_serialization.cpp
    ${FILESYSTEM_SOURCES}
)

target_include_directories(termihui_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Platform-specific includes and frameworks
if(APPLE)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    target_link_libraries(termihui_shared PUBLIC ${FOUNDATION_FRAMEWORK})
else()
    target_include_directories(termihui_shared PRIVATE ${platform_folders_SOURCE_DIR})
    target_link_libraries(termihui_shared PRIVATE platform_folders)
endif()

target_link_libraries(termihui_shared
    PUBLIC
        hv_static
        fmt::fmt
)

# Optional installation
install(TARGETS termihui_shared
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)

# =============================================================================
# Unit Tests
# =============================================================================

# Fetch Catch2 for testing
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(Catch2)

# Test sources
set(TEST_SOURCES
    tests/test_grid2d.cpp
)

add_executable(shared_unit_tests ${TEST_SOURCES})

target_link_libraries(shared_unit_tests PRIVATE
    Catch2::Catch2WithMain
    termihui_shared
)

target_include_directories(shared_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Enable CTest
enable_testing()
add_test(NAME shared_unit_tests COMMAND shared_unit_tests)
