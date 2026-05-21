cmake_minimum_required(VERSION 3.16)
project(pzem_logger_tests CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Test executable ──────────────────────────────────────────────────────────
add_executable(test_logger test_logger.cpp)

# -isystem forces GCC to search these directories for ALL #include <...>
# lookups, ahead of real system paths. This is what intercepts
# #include <Arduino.h>, <SPI.h>, <SD.h> etc. and redirects to our stubs.
# /usr/include is already a system path so <doctest/doctest.h> resolves
# without any extra entry.
target_compile_options(test_logger PRIVATE
    -isystem ${CMAKE_CURRENT_SOURCE_DIR}/mocks
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -O0 -g
)

# arduino_mock.h is included with quotes from the stub headers, so a
# regular include_directories entry is enough for it.
target_include_directories(test_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}                    # arduino_mock.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../PZEM_Logger  # Config.h, Logger.h
)

# ── CTest integration ────────────────────────────────────────────────────────
enable_testing()
add_test(NAME unit_tests       COMMAND test_logger -v)
add_test(NAME unit_tests_junit COMMAND test_logger --reporters=junit --out=test_results.xml)
