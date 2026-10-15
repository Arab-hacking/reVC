cmake_minimum_required(VERSION 3.14)
project(reVC_DebugLog_Tests CXX)

find_package(Threads REQUIRED)
add_executable(debug_log_tests DebugLogTests.cpp ../src/core/DebugLog.cpp)
target_include_directories(debug_log_tests PRIVATE ../src/core)
target_link_libraries(debug_log_tests PRIVATE Threads::Threads)
set_target_properties(debug_log_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)

add_executable(custom_ifp_tests CustomIfpTests.cpp)
set_target_properties(custom_ifp_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)

enable_testing()
add_test(NAME debug_log_tests COMMAND debug_log_tests)
add_test(NAME custom_ifp_tests COMMAND custom_ifp_tests)
