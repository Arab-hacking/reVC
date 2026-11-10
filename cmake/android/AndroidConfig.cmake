# Android-specific CMake configuration shared by the game target and the
# launcher wrapper project.
#
# This module is included from the top-level CMakeLists.txt when the build is
# configured by the Android NDK toolchain (i.e. when ANDROID is set). It only
# defines the defaults that the AGP wrapper project relies on; every value can
# still be overridden from the Gradle `externalNativeBuild.cmake.arguments`
# list or on the command line.

if(NOT ANDROID)
    return()
endif()

# The launcher wrapper project already provides the prebuilt imported targets
# for these libraries. Set these options explicitly so that a top-level Android
# configure does not fall back to host find_package() lookups which cannot work
# for cross-compiled targets.
set(REVC_ANDROID ON CACHE BOOL "Configure reVC for Android" FORCE)

# Native logging (liblog), GLES3 and OpenSL ES are provided by the platform.
set(REVC_ANDROID_PLATFORM_LIBS
    log
    GLESv3
    OpenSLES
    z
    CACHE STRING "Android platform libraries linked into the game")

# The prebuilt SDL2/mpg123/OpenAL binaries that ship in vendor/ are 32/64-bit
# ARM only; reject anything else early instead of failing later in the linker.
if(ANDROID_ABI AND NOT ANDROID_ABI MATCHES "^(armeabi-v7a|arm64-v8a)$")
    message(FATAL_ERROR
        "Unsupported Android ABI '${ANDROID_ABI}'. Only armeabi-v7a and "
        "arm64-v8a prebuilt dependencies are shipped with reVC.")
endif()

# Both ABIs are Release builds by default; keep the debug info in the build
# tree but let Gradle strip the packaged shared libraries.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# librw has no host-side tools to run while cross-compiling.
set(LIBRW_TOOLS OFF CACHE BOOL "No librw tools in Android build" FORCE)
set(LIBRW_INSTALL OFF CACHE BOOL "No librw install in Android build" FORCE)
set(REVC_INSTALL OFF CACHE BOOL "No CPack install rules in Android build" FORCE)
