cmake_minimum_required(VERSION 3.16)
project(angsh VERSION 0.1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to an optimized build unless the user picks something else.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra -pedantic)

add_executable(angsh
    src/main.cpp
    src/util.cpp
    src/env.cpp
    src/lexer.cpp
    src/expand.cpp
    src/parser.cpp
    src/executor.cpp
    src/builtins.cpp
    src/prompt.cpp
    src/config.cpp
    src/editor.cpp
    src/highlight.cpp
)

target_include_directories(angsh PRIVATE src)

# No third-party deps in v0.1.0 (stdlib + POSIX only).
# Install: `cmake -B build && cmake --build build && sudo cmake --install build`

install(TARGETS angsh DESTINATION bin)
install(DIRECTORY examples/ DESTINATION share/doc/angsh/examples)
install(DIRECTORY themes/ DESTINATION share/angsh/themes)

# --version output: default to the project version, overridable for releases
# (e.g. `cmake -B build -DANG_VERSION=0.2.0` from a tag).
if(NOT DEFINED ANG_VERSION)
    set(ANG_VERSION ${PROJECT_VERSION})
endif()
target_compile_definitions(angsh PRIVATE ANG_VERSION="${ANG_VERSION}")
