cmake_minimum_required(VERSION 3.16)
project(termihui_shared VERSION 1.0.0 LANGUAGES CXX)

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

# Static library
add_library(termihui_shared STATIC
    src/json_serialization.cpp
)

target_include_directories(termihui_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(termihui_shared
    PUBLIC
        hv_static
)

# Optional installation
install(TARGETS termihui_shared
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)
