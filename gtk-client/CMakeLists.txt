cmake_minimum_required(VERSION 3.20)
project(termihui-gtk-client VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find gtkmm-4.0
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)

# Add client-core library
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../client-core ${CMAKE_CURRENT_BINARY_DIR}/client-core)

# Source files
set(SOURCES
    src/main.cpp
    src/Application.cpp
    src/MainWindow.cpp
    src/ClientCoreWrapper.cpp
    src/WelcomeView.cpp
    src/ConnectingView.cpp
    src/TerminalView.cpp
    src/CommandBlockWidget.cpp
    src/PangoUtils.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${GTKMM_LIBRARIES}
    termihui_client_core
    fmt::fmt
)

# GTKmm compile flags
target_compile_options(${PROJECT_NAME} PRIVATE ${GTKMM_CFLAGS_OTHER})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${GTKMM_INCLUDE_DIRS}
)
