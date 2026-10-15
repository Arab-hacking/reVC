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

# The real animation code can be tested without game assets by supplying small
# RenderWare/streaming stubs and synthetic skeletons. Dead-code elimination
# keeps the remaining game-only dependencies out of this standalone test.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_executable(anim_association_nodes_tests
        AnimAssociationNodesTests.cpp
        ../src/animation/AnimBlendAssociation.cpp
        ../src/animation/AnimBlendNode.cpp)
    target_compile_definitions(anim_association_nodes_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(anim_association_nodes_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src/fakerw ../src/core ../src/math ../src/skel ../src/animation
        ../src/modelinfo ../src/renderer ../src/rw)
    target_compile_options(anim_association_nodes_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(anim_association_nodes_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(anim_association_nodes_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME anim_association_nodes_tests COMMAND anim_association_nodes_tests)
endif()
