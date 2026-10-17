cmake_minimum_required(VERSION 3.16)
project(angsh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
