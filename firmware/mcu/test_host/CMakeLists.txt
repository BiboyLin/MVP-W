cmake_minimum_required(VERSION 3.16)
project(mcu_host_test C)
set(CMAKE_C_STANDARD 99)

# Fetch Unity test framework (MIT, ThrowTheSwitch)
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(unity)

# ── test: servo math ────────────────────────────────────────────────
add_executable(test_servo_math
    test_servo_math.c
    ../main/servo_math.c
)
target_include_directories(test_servo_math PRIVATE ../main)
target_link_libraries(test_servo_math unity)

# ── test: uart protocol ─────────────────────────────────────────────
add_executable(test_uart_protocol
    test_uart_protocol.c
    ../main/uart_protocol.c
)
target_include_directories(test_uart_protocol PRIVATE ../main)
target_link_libraries(test_uart_protocol unity)

# ── CTest integration ───────────────────────────────────────────────
enable_testing()
add_test(NAME ServoMath    COMMAND test_servo_math)
add_test(NAME UartProtocol COMMAND test_uart_protocol)
