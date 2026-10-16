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

add_executable(custom_asset_cache_tests
    CustomAssetCacheTests.cpp ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(custom_asset_cache_tests PRIVATE ../src/extras/custom ../src/extras/custom/br)
set_target_properties(custom_asset_cache_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_asset_cache_tests COMMAND custom_asset_cache_tests)

add_executable(background_asset_queue_tests
    BackgroundAssetQueueTests.cpp ../src/extras/custom/CustomZip.cpp
    ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(background_asset_queue_tests PRIVATE
    ../tools/custom-folder-verification/shim
    ../src/extras/custom ../src/extras/custom/br)
target_link_libraries(background_asset_queue_tests PRIVATE Threads::Threads)
set_target_properties(background_asset_queue_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME background_asset_queue_tests COMMAND background_asset_queue_tests)

add_executable(custom_world_format_tests
    CustomWorldFormatTests.cpp ../src/extras/custom/CustomZip.cpp)
target_include_directories(custom_world_format_tests PRIVATE
    ../tools/custom-folder-verification/shim ../src/extras/custom)
set_target_properties(custom_world_format_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_world_format_tests COMMAND custom_world_format_tests)

add_executable(custom_img_collision_tests CustomImgCollisionTests.cpp)
set_target_properties(custom_img_collision_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_img_collision_tests COMMAND custom_img_collision_tests)

# The real animation code can be tested without game assets by supplying small
# RenderWare/streaming stubs and synthetic skeletons. Dead-code elimination
# keeps the remaining game-only dependencies out of this standalone test.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Read an actual synthetic RW 3.6 DFF in a headless librw engine. This
    # tests clump geometry/material ownership instead of mocking the API.
    set(LIBRW_PLATFORM "NULL" CACHE STRING "Headless librw for tests" FORCE)
    set(LIBRW_TOOLS OFF CACHE BOOL "No librw utilities in tests" FORCE)
    set(LIBRW_INSTALL OFF CACHE BOOL "No librw install in tests" FORCE)
    add_subdirectory(../vendor/librw ${CMAKE_CURRENT_BINARY_DIR}/librw-static)
    add_executable(sa_static_dff_tests
        SAStaticDffTests.cpp ../src/extras/custom/SAStaticModel.cpp)
    target_compile_definitions(sa_static_dff_tests PRIVATE LIBRW)
    target_include_directories(sa_static_dff_tests PRIVATE ../src/extras/custom)
    target_link_libraries(sa_static_dff_tests PRIVATE librw::librw)
    set_target_properties(sa_static_dff_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME sa_static_dff_tests COMMAND sa_static_dff_tests)

    add_executable(txd_store_capacity_tests
        TxdStoreCapacityTests.cpp ../src/rw/TxdStore.cpp)
    target_compile_definitions(txd_store_capacity_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(txd_store_capacity_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(txd_store_capacity_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(txd_store_capacity_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(txd_store_capacity_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME txd_store_capacity_tests COMMAND txd_store_capacity_tests)

    # Link the real ZIP/IMG scanner and both collision converters. The game
    # dependencies not exercised by this fixture are discarded by section GC.
    add_executable(custom_archive_integration_tests
        CustomArchiveIntegrationTests.cpp
        ../src/extras/custom/CustomModels.cpp
        ../src/extras/custom/CustomZip.cpp
        ../src/extras/custom/CustomCol.cpp
        ../src/rw/TxdStore.cpp
        ../src/extras/custom/br/third_party/astc_decomp.cpp)
    target_compile_definitions(custom_archive_integration_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(custom_archive_integration_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(custom_archive_integration_tests PRIVATE
        -ffunction-sections -fdata-sections -Wno-write-strings -Wno-multichar)
    target_link_options(custom_archive_integration_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(custom_archive_integration_tests PRIVATE Threads::Threads)
    set_target_properties(custom_archive_integration_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME custom_archive_integration_tests COMMAND custom_archive_integration_tests)

    add_executable(anim_association_nodes_tests
        AnimAssociationNodesTests.cpp
        ../src/animation/AnimBlendAssociation.cpp
        ../src/animation/AnimBlendNode.cpp
        ../src/animation/Bones.cpp)
    target_compile_definitions(anim_association_nodes_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(anim_association_nodes_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src/fakerw ../src/core ../src/math ../src/skel ../src/animation
        ../src/modelinfo ../src/renderer ../src/rw ../src/collision ../src/peds)
    target_compile_options(anim_association_nodes_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(anim_association_nodes_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(anim_association_nodes_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME anim_association_nodes_tests COMMAND anim_association_nodes_tests)

    # Link the actual ANPK/ANP3 reader with a memory-stream shim. Section GC
    # keeps the unrelated game/renderer entry points out of this fixture.
    set(ANIM_FILE_READER_SOURCES
        AnimFileReadTests.cpp
        ../src/animation/AnimManager.cpp
        ../src/animation/AnimBlendHierarchy.cpp
        ../src/animation/AnimBlendSequence.cpp
        ../src/math/Quaternion.cpp)
    add_executable(anim_file_read_tests ${ANIM_FILE_READER_SOURCES})
    add_executable(anim_file_read_compressed_tests ${ANIM_FILE_READER_SOURCES})
    foreach(reader_test anim_file_read_tests anim_file_read_compressed_tests)
    target_compile_definitions(${reader_test} PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(${reader_test} PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(${reader_test} PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(${reader_test} PRIVATE -Wl,--gc-sections)
    set_target_properties(${reader_test} PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME ${reader_test} COMMAND ${reader_test})
    endforeach()
    target_compile_definitions(anim_file_read_compressed_tests PRIVATE ANIM_COMPRESSION)
endif()
