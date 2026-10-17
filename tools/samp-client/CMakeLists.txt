cmake_minimum_required(VERSION 3.14)
project(samp_client LANGUAGES CXX)

# Deliberately standalone: no headers, libraries, hooks or assets from reVC.
add_library(samp_query STATIC SampQuery.cpp SampQuery.h)
target_include_directories(samp_query PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(samp_query PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
if(WIN32)
    target_link_libraries(samp_query PUBLIC ws2_32)
endif()

find_package(Threads REQUIRED)
add_executable(samp_probe SampProbe.cpp)
target_link_libraries(samp_probe PRIVATE samp_query Threads::Threads)
set_target_properties(samp_probe PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON
                                           OUTPUT_NAME samp-probe)

include(CTest)
if(BUILD_TESTING)
    add_executable(samp_query_tests SampQueryTests.cpp)
    target_link_libraries(samp_query_tests PRIVATE samp_query Threads::Threads)
    set_target_properties(samp_query_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME samp_query_tests COMMAND samp_query_tests)
endif()
