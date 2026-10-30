cmake_minimum_required(VERSION 3.22.1)
project(reVC_Android LANGUAGES C CXX)

# The launcher used to produce an APK with no native game at all. Build the
# actual reVC shared library here, using the Android prebuilts already shipped
# with the repository. Keep imported paths inside the build directory so AGP
# also packages these dependencies into the APK for each configured ABI.
get_filename_component(REVC_GAME_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(REVC_VENDOR_DIR "${REVC_GAME_DIR}/vendor")
if(NOT EXISTS "${REVC_GAME_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Missing reVC sources: ${REVC_GAME_DIR}")
endif()
if(NOT ANDROID_ABI MATCHES "^(armeabi-v7a|arm64-v8a)$")
    message(FATAL_ERROR "Unsupported Android ABI: ${ANDROID_ABI}")
endif()

set(PREBUILT_DIR "${CMAKE_CURRENT_BINARY_DIR}/prebuilt")
function(revc_android_prebuilt target source filename headers)
    if(NOT EXISTS "${source}")
        message(FATAL_ERROR "Missing Android dependency: ${source}")
    endif()
    configure_file("${source}" "${PREBUILT_DIR}/${filename}" COPYONLY)
    add_library(${target} SHARED IMPORTED GLOBAL)
    set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION "${PREBUILT_DIR}/${filename}"
        INTERFACE_INCLUDE_DIRECTORIES "${headers}")
endfunction()

revc_android_prebuilt(SDL2::SDL2
    "${REVC_VENDOR_DIR}/sdl2/libs/Android/${ANDROID_ABI}/libSDL2.so"
    libSDL2.so "${REVC_VENDOR_DIR}/sdl2/include")
revc_android_prebuilt(MPG123::libmpg123
    "${REVC_VENDOR_DIR}/mpg123/lib/Android/${ANDROID_ABI}/libmpg123.so"
    libmpg123.so "${REVC_VENDOR_DIR}/mpg123/include")

# The checked-in OpenAL binary is called libopenal.so, but its DT_SONAME is
# libOpenAL32.so / libOpenAL64.so. The dynamic loader needs that exact name.
if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(OPENAL_SONAME libOpenAL64.so)
else()
    set(OPENAL_SONAME libOpenAL32.so)
endif()
revc_android_prebuilt(OpenAL::OpenAL
    "${REVC_VENDOR_DIR}/openal-soft/libs/Android/${ANDROID_ABI}/libopenal.so"
    "${OPENAL_SONAME}" "${REVC_VENDOR_DIR}/openal-soft/include")

set(LIBRW_TOOLS OFF CACHE BOOL "No librw tools in Android APK" FORCE)
set(LIBRW_INSTALL OFF CACHE BOOL "No librw install in Android APK" FORCE)
add_subdirectory("${REVC_GAME_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/game_build")
if(NOT TARGET reVC)
    message(FATAL_ERROR "The Android APK must build the reVC native game target")
endif()
# SDLActivity loads 'revc' and looks for librevc.so, not libreVC.so.
set_target_properties(reVC PROPERTIES OUTPUT_NAME revc)
