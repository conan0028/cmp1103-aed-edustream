cmake_minimum_required(VERSION 3.16)
project(edustream LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Tipo de build" FORCE)
endif()

option(BUILD_GUI "Compilar interfaces GTK4" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0>=1.16)
if(BUILD_GUI)
    pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
endif()

add_library(edustream_core STATIC src/core/transmitter.cpp src/core/receiver.cpp)
target_include_directories(edustream_core PUBLIC src/core)
target_link_libraries(edustream_core PUBLIC PkgConfig::GST)

add_executable(edustream_transmissor_cli src/cli/transmissor_cli.cpp)
target_link_libraries(edustream_transmissor_cli PRIVATE edustream_core)
add_executable(edustream_receptor_cli src/cli/receptor_cli.cpp)
target_link_libraries(edustream_receptor_cli PRIVATE edustream_core)

if(BUILD_GUI)
    add_executable(edustream_professor src/ui/professor_gui.cpp)
    target_link_libraries(edustream_professor PRIVATE edustream_core PkgConfig::GTK4)

    add_executable(edustream_aluno src/ui/aluno_gui.cpp)
    target_link_libraries(edustream_aluno PRIVATE edustream_core PkgConfig::GTK4)
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(edustream_core PRIVATE -Wall -Wextra)
    target_compile_options(edustream_transmissor_cli PRIVATE -Wall -Wextra)
    target_compile_options(edustream_receptor_cli PRIVATE -Wall -Wextra)
    if(BUILD_GUI)
        target_compile_options(edustream_professor PRIVATE -Wall -Wextra)
        target_compile_options(edustream_aluno PRIVATE -Wall -Wextra)
    endif()
endif()
