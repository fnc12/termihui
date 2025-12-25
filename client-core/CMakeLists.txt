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

# Создаём статическую библиотеку
add_library(termihui_client_core STATIC
    src/client_core.cpp
    src/ansi_parser.cpp
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
