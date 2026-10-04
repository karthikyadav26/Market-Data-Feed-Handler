cmake_minimum_required(VERSION 3.16)
project(market_data_feed_handler VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_SANITIZERS "Build with ASan/UBSan" OFF)
option(ENABLE_DPDK "Enable a future DPDK adapter (not implemented in this release)" OFF)

if(ENABLE_DPDK)
    message(FATAL_ERROR "ENABLE_DPDK is intentionally unavailable: no fake DPDK implementation is shipped. Use the transport abstraction documented in docs/architecture.md to add a real adapter.")
endif()

add_library(feed_core
    src/pipeline.cpp
    src/encoder.cpp
    src/decoder.cpp
    src/sequence_validator.cpp
    src/local_order_book.cpp
    src/udp_transport.cpp
    src/recorder.cpp
    src/csv_feed.cpp
    src/csv_output.cpp
)

target_include_directories(feed_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(feed_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
target_link_libraries(feed_core PUBLIC pthread)

if(ENABLE_SANITIZERS)
    target_compile_options(feed_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(feed_core PUBLIC -fsanitize=address,undefined)
endif()

add_executable(market_data_simulator src/simulator_main.cpp)
target_link_libraries(market_data_simulator PRIVATE feed_core)

add_executable(feed_handler src/feed_handler_main.cpp)
target_link_libraries(feed_handler PRIVATE feed_core)

add_executable(feed_replay src/replay_main.cpp)
target_link_libraries(feed_replay PRIVATE feed_core)

add_executable(feed_recorder src/recorder_main.cpp)
target_link_libraries(feed_recorder PRIVATE feed_core)

add_executable(feed_benchmark benchmarks/feed_benchmark.cpp)
target_link_libraries(feed_benchmark PRIVATE feed_core)

enable_testing()
add_executable(feed_unit_tests
    tests/test_main.cpp
    tests/test_protocol.cpp
    tests/test_decoder.cpp
    tests/test_sequence.cpp
    tests/test_order_book.cpp
    tests/test_queue.cpp
    tests/test_replay.cpp
)
target_link_libraries(feed_unit_tests PRIVATE feed_core)

target_compile_options(feed_unit_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
if(ENABLE_SANITIZERS)
    target_compile_options(feed_unit_tests PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(feed_unit_tests PUBLIC -fsanitize=address,undefined)
endif()

add_test(NAME unit_tests COMMAND feed_unit_tests)
add_test(NAME regression COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_regression.sh ${CMAKE_CURRENT_BINARY_DIR})
add_test(NAME integration COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_integration.sh ${CMAKE_CURRENT_BINARY_DIR})

install(TARGETS market_data_simulator feed_handler feed_replay feed_recorder feed_benchmark RUNTIME DESTINATION bin)
