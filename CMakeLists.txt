# RailGuard — Indian Railways cascade delay modeling (C++17, STL only)
cmake_minimum_required(VERSION 3.16)
project(RailGuard CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options(-Wall -Wextra -O2)

set(RG_SOURCES
    src/graph/NetworkGraph.cpp
    src/graph/TarjanBridge.cpp
    src/cascade/DelayDAG.cpp
    src/cascade/DSUWithRollback.cpp
    src/analytics/SlidingWindow.cpp
    src/analytics/PassengerImpact.cpp
    src/intervention/SpragueGrundy.cpp
    src/intervention/InterventionOptimizer.cpp
    src/io/CSVParser.cpp
    src/io/NTESFeedSimulator.cpp
)

add_library(railguard_core STATIC ${RG_SOURCES})
target_include_directories(railguard_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(railguard src/main.cpp)
target_link_libraries(railguard PRIVATE railguard_core)

# --- Unit tests (no framework; assert + return) ---
add_executable(test_tarjan tests/test_tarjan.cpp)
target_link_libraries(test_tarjan PRIVATE railguard_core)

add_executable(test_dsu tests/test_dsu.cpp)
target_link_libraries(test_dsu PRIVATE railguard_core)

add_executable(test_cascade tests/test_cascade.cpp)
target_link_libraries(test_cascade PRIVATE railguard_core)

add_executable(test_sprague tests/test_sprague.cpp)
target_link_libraries(test_sprague PRIVATE railguard_core)

enable_testing()
add_test(NAME tarjan COMMAND test_tarjan)
add_test(NAME dsu_rollback COMMAND test_dsu)
add_test(NAME cascade COMMAND test_cascade)
set_tests_properties(cascade PROPERTIES ENVIRONMENT "RAILGUARD_DATA=${CMAKE_SOURCE_DIR}/data")
add_test(NAME sprague COMMAND test_sprague)
