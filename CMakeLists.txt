cmake_minimum_required(VERSION 3.10)

project(HelixKV)

set(CMAKE_CXX_STANDARD 20)

add_executable(
    helixkv
    src/main.cpp
    src/server.cpp
    src/command_handler.cpp
    src/storage.cpp
    src/persistence.cpp
)

find_package(Threads REQUIRED)

target_link_libraries(
    helixkv
    Threads::Threads
)