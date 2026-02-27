cmake_minimum_required(VERSION 3.16)
project(MVP-W_S3_Test C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Unity test framework
include(FetchContent)
FetchContent_Declare(
    Unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.5.2
)
FetchContent_MakeAvailable(Unity)

# Source files (pure logic, no ESP-IDF dependencies)
set(SOURCES
    ../main/ws_router.c
    ../main/cJSON.c
)

# Test files
set(TEST_SOURCES
    test_ws_router.c
)

# Executable
add_executable(test_runner
    ${SOURCES}
    ${TEST_SOURCES}
)

target_include_directories(test_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../main
    ${unity_SOURCE_DIR}/src
)

target_link_libraries(test_runner PRIVATE unity)

# CTest
enable_testing()
add_test(NAME WS_Router COMMAND test_runner)

# Add memory check target (optional, requires valgrind)
add_custom_target(memcheck
    COMMAND valgrind --leak-check=full --error-exitcode=1 $<TARGET_FILE:test_runner>
    DEPENDS test_runner
)
