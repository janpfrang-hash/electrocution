cmake_minimum_required(VERSION 3.16)
project(pzem_logger_tests CXX)
 
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
# ── Find doctest (installed via apt: doctest-dev) ────────────────────────────
find_path(DOCTEST_INCLUDE_DIR doctest/doctest.h
    PATHS /usr/include /usr/local/include
    REQUIRED
)
 
# ── Test executable ──────────────────────────────────────────────────────────
add_executable(test_logger test_logger.cpp)
 
# IMPORTANT: mocks/ must come BEFORE the sketch directory so that
# #include <Arduino.h>, <SPI.h> etc. resolve to our stubs, not the real HAL.
# target_include_directories with BEFORE ensures ordering even vs system paths.
target_include_directories(test_logger BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/mocks        # Arduino.h, SPI.h, SD.h, FS.h, PZEM004Tv30.h
    ${CMAKE_CURRENT_SOURCE_DIR}              # arduino_mock.h
    ${DOCTEST_INCLUDE_DIR}/doctest           # doctest.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../PZEM_Logger  # Config.h, Logger.h
)
 
target_compile_options(test_logger PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -O0 -g
)
 
# ── CTest integration ────────────────────────────────────────────────────────
enable_testing()
add_test(NAME unit_tests       COMMAND test_logger -v)
add_test(NAME unit_tests_junit COMMAND test_logger --reporters=junit --out=test_results.xml)

