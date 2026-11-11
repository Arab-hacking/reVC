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
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    # Check GLSL sources against the compiled .inc strings. When a GLSL
    # validator is installed the test also links the real GL3 shader variants.
    add_test(NAME gl3_shader_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/GL3ShaderTests.py)
endif()
add_executable(gl3_atmosphere_tests GL3AtmosphereTests.cpp)
set_target_properties(gl3_atmosphere_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME gl3_atmosphere_tests COMMAND gl3_atmosphere_tests)

add_executable(sun_shadow_lighting_tests SunShadowLightingTests.cpp)
set_target_properties(sun_shadow_lighting_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME sun_shadow_lighting_tests COMMAND sun_shadow_lighting_tests)
add_test(NAME debug_log_tests COMMAND debug_log_tests)
add_test(NAME custom_ifp_tests COMMAND custom_ifp_tests)

add_executable(noclip_movement_tests NoClipMovementTests.cpp)
set_target_properties(noclip_movement_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME noclip_movement_tests COMMAND noclip_movement_tests)

# Black Russia graphics parity: constants and formulas of the client's
# reflection, fog and post-processing shaders (PUNCT ANALYSE v38, graphic 09/12/13).
add_executable(graphics_parity_tests GraphicsParityTests.cpp)
target_include_directories(graphics_parity_tests PRIVATE ../src/extras/custom)
set_target_properties(graphics_parity_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME graphics_parity_tests COMMAND graphics_parity_tests)

add_executable(mobile_touch_math_tests MobileTouchMathTests.cpp)
target_include_directories(mobile_touch_math_tests PRIVATE ../src/extras/custom)
set_target_properties(mobile_touch_math_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME mobile_touch_math_tests COMMAND mobile_touch_math_tests)

add_executable(br_archive_parity_tests BrArchiveParityTests.cpp)
target_include_directories(br_archive_parity_tests PRIVATE ../src/extras/custom)
set_target_properties(br_archive_parity_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME br_archive_parity_tests COMMAND br_archive_parity_tests)

add_executable(native_map_options_tests NativeMapOptionsTests.cpp)
set_target_properties(native_map_options_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME native_map_options_tests COMMAND native_map_options_tests)

add_executable(player_model_cycle_tests PlayerModelCycleTests.cpp)
set_target_properties(player_model_cycle_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME player_model_cycle_tests COMMAND player_model_cycle_tests)

add_executable(world_sector_math_tests WorldSectorMathTests.cpp)
set_target_properties(world_sector_math_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME world_sector_math_tests COMMAND world_sector_math_tests)

add_executable(world_recovery_tests WorldRecoveryTests.cpp)
set_target_properties(world_recovery_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME world_recovery_tests COMMAND world_recovery_tests)

add_executable(bullet_travel_tests BulletTravelTests.cpp)
set_target_properties(bullet_travel_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME bullet_travel_tests COMMAND bullet_travel_tests)

add_executable(frame_render_list_tests FrameRenderListTests.cpp)
set_target_properties(frame_render_list_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME frame_render_list_tests COMMAND frame_render_list_tests)

add_executable(audio_entity_slots_tests AudioEntitySlotsTests.cpp)
set_target_properties(audio_entity_slots_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME audio_entity_slots_tests COMMAND audio_entity_slots_tests)

add_executable(save_block_bounds_tests SaveBlockBoundsTests.cpp)
set_target_properties(save_block_bounds_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME save_block_bounds_tests COMMAND save_block_bounds_tests)

# Validate the actual VC save-pool wire framing before loading entities with
# model IDs from an IDE configuration different from the current map.
add_executable(saved_pool_validation_tests SavedPoolValidationTests.cpp)
set_target_properties(saved_pool_validation_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME saved_pool_validation_tests COMMAND saved_pool_validation_tests)

add_executable(custom_asset_cache_tests
    CustomAssetCacheTests.cpp ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(custom_asset_cache_tests PRIVATE ../src/extras/custom ../src/extras/custom/br)
set_target_properties(custom_asset_cache_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_asset_cache_tests COMMAND custom_asset_cache_tests)

add_executable(graphics_btx_tests
    GraphicsBtxTests.cpp ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(graphics_btx_tests PRIVATE ../src/extras/custom ../src/extras/custom/br)
set_target_properties(graphics_btx_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME graphics_btx_tests COMMAND graphics_btx_tests)

add_executable(background_asset_queue_tests
    BackgroundAssetQueueTests.cpp ../src/extras/custom/CustomZip.cpp
    ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(background_asset_queue_tests PRIVATE
    ../tools/custom-folder-verification/shim
    ../src/extras/custom ../src/extras/custom/br)
target_link_libraries(background_asset_queue_tests PRIVATE Threads::Threads)
set_target_properties(background_asset_queue_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME background_asset_queue_tests COMMAND background_asset_queue_tests)

add_executable(bpc_pipeline_tests
    BpcPipelineTests.cpp ../src/extras/custom/CustomZip.cpp
    ../src/extras/custom/br/third_party/astc_decomp.cpp)
target_include_directories(bpc_pipeline_tests PRIVATE
    ../tools/custom-folder-verification/shim
    ../src/extras/custom ../src/extras/custom/br)
target_link_libraries(bpc_pipeline_tests PRIVATE Threads::Threads)
set_target_properties(bpc_pipeline_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME bpc_pipeline_tests COMMAND bpc_pipeline_tests)

add_executable(custom_water_tests CustomWaterTests.cpp)
set_target_properties(custom_water_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_water_tests COMMAND custom_water_tests)

add_executable(custom_world_format_tests
    CustomWorldFormatTests.cpp ../src/extras/custom/CustomZip.cpp)
target_include_directories(custom_world_format_tests PRIVATE
    ../tools/custom-folder-verification/shim ../src/extras/custom)
set_target_properties(custom_world_format_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME custom_world_format_tests COMMAND custom_world_format_tests)

add_executable(expandable_pool_tests ExpandablePoolTests.cpp)
set_target_properties(expandable_pool_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME expandable_pool_tests COMMAND expandable_pool_tests)

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

    # Compare the BR IDE/IPL reader and VC's librw axis/angle rotation against
    # SA's conjugated quaternion matrix on real map lines (including w < 0).
    add_executable(custom_world_rotation_tests CustomWorldRotationTests.cpp)
    target_include_directories(custom_world_rotation_tests PRIVATE ../src/extras/custom)
    target_link_libraries(custom_world_rotation_tests PRIVATE librw::librw)
    set_target_properties(custom_world_rotation_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME custom_world_rotation_tests COMMAND custom_world_rotation_tests)

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

    add_executable(sky_vehicle_txd_tests
        SkyVehicleTxdTests.cpp ../src/rw/TxdStore.cpp)
    target_compile_definitions(sky_vehicle_txd_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(sky_vehicle_txd_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(sky_vehicle_txd_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(sky_vehicle_txd_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(sky_vehicle_txd_tests PRIVATE librw::librw)
    set_target_properties(sky_vehicle_txd_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME sky_vehicle_txd_tests COMMAND sky_vehicle_txd_tests)

    # Exercise the real VC escalator initializer when its DATA/MAPS step model
    # is missing (and when the native map is enabled with a valid model).
    add_executable(escalator_init_tests
        EscalatorInitTests.cpp ../src/renderer/Fluff.cpp
        ../src/math/Matrix.cpp ../src/math/Vector.cpp)
    target_compile_definitions(escalator_init_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(escalator_init_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(escalator_init_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(escalator_init_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(escalator_init_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME escalator_init_tests COMMAND escalator_init_tests)

    # Test the model preflight for the Release D3D9 pickup crash: without
    # native DATA/MAPS IDEs, scripts can request -1 or an unregistered ID.
    add_executable(pickup_missing_model_tests
        PickupMissingModelTests.cpp ../src/control/Pickups.cpp
        ../src/modelinfo/ModelIndices.cpp)
    target_compile_definitions(pickup_missing_model_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(pickup_missing_model_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(pickup_missing_model_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(pickup_missing_model_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(pickup_missing_model_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME pickup_missing_model_tests COMMAND pickup_missing_model_tests)

    # Exercise CDirectory::AddItem itself: the extra-objects directory must
    # retain native DFFs beyond its old 256-entry reserve when VC IDEs are off.
    add_executable(directory_growth_tests
        DirectoryGrowthTests.cpp ../src/core/Directory.cpp)
    target_compile_definitions(directory_growth_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(directory_growth_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(directory_growth_tests PRIVATE -ffunction-sections -fdata-sections)
    target_link_options(directory_growth_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(directory_growth_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME directory_growth_tests COMMAND directory_growth_tests)

    # Link the real IMG/DIR scanner and selective custom/common.zip reader.
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

    # Memory-pressure optimization: the 48-parameter table, the RAM -> budget
    # mapping, the pressure function and the hysteresis state machine. Header
    # only, so the test builds without the engine.
    add_executable(optimization_profile_tests OptimizationProfileTests.cpp)
    target_include_directories(optimization_profile_tests PRIVATE ../src/extras/custom)
    set_target_properties(optimization_profile_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME optimization_profile_tests COMMAND optimization_profile_tests)

    # Collision ceilings that the client asserts on (129 spheres, 1600 tris)
    # and the bounds the loader applies to the counts it reads from a file.
    add_executable(collision_count_limit_tests CollisionCountLimitsTests.cpp)
    target_include_directories(collision_count_limit_tests PRIVATE ../src/collision)
    set_target_properties(collision_count_limit_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME collision_count_limit_tests COMMAND collision_count_limit_tests)

    # Several collision models per container (.col and .cls), including the
    # sector-padded slices an IMG actually hands over.
    add_executable(collision_container_tests CollisionContainerTests.cpp)
    target_include_directories(collision_container_tests PRIVATE ../src/extras/custom)
    set_target_properties(collision_container_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME collision_container_tests COMMAND collision_container_tests)

    # The collision slot policy: which store slots a frame wants loaded. It was
    # inlined in CColStore::LoadCollision next to a per-slot scan of the mission
    # cleanup list, so it could not be tested or reasoned about on its own.
    add_executable(col_slot_selection_tests ColSlotSelectionTests.cpp)
    target_include_directories(col_slot_selection_tests PRIVATE
        ../src ../src/collision ../src/math)
    set_target_properties(col_slot_selection_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME col_slot_selection_tests COMMAND col_slot_selection_tests)

    # The COL2/COL3 TBounds block stores the bounding sphere in one of two
    # orders depending on which tool wrote the file; the converter has to
    # recognise it instead of guessing from the platform.
    add_executable(custom_col_bounds_tests
        CustomColBoundsTests.cpp
        ../src/extras/custom/CustomCol.cpp)
    target_compile_definitions(custom_col_bounds_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(custom_col_bounds_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(custom_col_bounds_tests PRIVATE
        -ffunction-sections -fdata-sections -Wno-write-strings -Wno-multichar)
    target_link_options(custom_col_bounds_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(custom_col_bounds_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME custom_col_bounds_tests COMMAND custom_col_bounds_tests)

    add_executable(android_bpc_archive_integration_tests
        AndroidBpcArchiveIntegrationTests.cpp
        ../src/extras/custom/CustomModels.cpp
        ../src/extras/custom/CustomZip.cpp
        ../src/extras/custom/CustomCol.cpp
        ../src/rw/TxdStore.cpp
        ../src/extras/custom/br/third_party/astc_decomp.cpp)
    target_compile_definitions(android_bpc_archive_integration_tests PRIVATE
        ANDROID LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(android_bpc_archive_integration_tests PRIVATE
        android_shim
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(android_bpc_archive_integration_tests PRIVATE
        -ffunction-sections -fdata-sections -Wno-write-strings -Wno-multichar)
    target_link_options(android_bpc_archive_integration_tests PRIVATE -Wl,--gc-sections)
    target_link_libraries(android_bpc_archive_integration_tests PRIVATE Threads::Threads)
    set_target_properties(android_bpc_archive_integration_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME android_bpc_archive_integration_tests COMMAND android_bpc_archive_integration_tests)

    add_executable(script_save_validation_tests
        ScriptSaveValidationTests.cpp
        ../src/control/Script5.cpp)
    target_compile_definitions(script_save_validation_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(script_save_validation_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(script_save_validation_tests PRIVATE -ffunction-sections -fdata-sections -Wno-write-strings -Wno-multichar)
    target_link_options(script_save_validation_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(script_save_validation_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME script_save_validation_tests COMMAND script_save_validation_tests)

    add_executable(ped_skeleton_classification_tests
        PedSkeletonClassificationTests.cpp
        ../src/modelinfo/PedModelInfo.cpp)
    target_compile_definitions(ped_skeleton_classification_tests PRIVATE
        LIBRW RW_GL3 LIBRW_SDL2 CMAKE_BUILD USE_OUR_VERSIONING AUDIO_OAL)
    target_include_directories(ped_skeleton_classification_tests PRIVATE
        ../vendor/librw ../vendor/librw/src/gl ../vendor/sdl2/include
        ../src ../src/animation ../src/audio ../src/buildings ../src/collision
        ../src/control ../src/core ../src/entities ../src/extras
        ../src/extras/custom ../src/extras/custom/br ../src/fakerw ../src/math
        ../src/modelinfo ../src/objects ../src/peds ../src/renderer ../src/rw
        ../src/save ../src/skel ../src/text ../src/vehicles ../src/weapons)
    target_compile_options(ped_skeleton_classification_tests PRIVATE -ffunction-sections -fdata-sections -Wno-write-strings)
    target_link_options(ped_skeleton_classification_tests PRIVATE -Wl,--gc-sections)
    set_target_properties(ped_skeleton_classification_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
    add_test(NAME ped_skeleton_classification_tests COMMAND ped_skeleton_classification_tests)

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

add_executable(test_graphics_tests TestGraphicsTests.cpp)
target_include_directories(test_graphics_tests PRIVATE ../src/extras/custom)
set_target_properties(test_graphics_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_graphics_tests COMMAND test_graphics_tests)

# The menu's .ini round-trip dereferences a row's value pointer for every row
# that has a save key, so a row built as `CCFODynamic(nil, "cat", "key", ...)`
# crashed the boot (SIGSEGV right after gta_vc.set). Source-level, because the
# menu itself cannot be instantiated without RenderWare.
add_executable(menu_ini_round_trip_tests MenuIniRoundTripTests.cpp)
set_target_properties(menu_ini_round_trip_tests PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED ON)
add_test(NAME menu_ini_round_trip_tests COMMAND menu_ini_round_trip_tests)
set_tests_properties(menu_ini_round_trip_tests PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
