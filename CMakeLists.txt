cmake_minimum_required(VERSION 3.10)

project(HelixKV)

set(CMAKE_CXX_STANDARD 20)

# Option to enable Sanitizers
option(ENABLE_SANITIES "Enable Address and Leak Sanitizers" ON)
if(ENABLE_SANITIES)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(-fsanitize=address,leak -g)
        add_link_options(-fsanitize=address,leak)
    endif()
endif()

include_directories(src)

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

if(WIN32)
    target_link_libraries(helixkv ws2_32)
endif()

# Automated unit/integration tests target
add_executable(
    helixkv_tests
    tests/test_main.cpp
    src/command_handler.cpp
    src/storage.cpp
    src/persistence.cpp
)
target_link_libraries(helixkv_tests Threads::Threads)
if(WIN32)
    target_link_libraries(helixkv_tests ws2_32)
endif()

# Concurrency stress tests target
add_executable(
    helixkv_stress_test
    tests/stress_test.cpp
)
target_link_libraries(helixkv_stress_test Threads::Threads)
if(WIN32)
    target_link_libraries(helixkv_stress_test ws2_32)
endif()