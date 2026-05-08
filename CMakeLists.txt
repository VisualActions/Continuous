cmake_minimum_required(VERSION 3.24)

# vcpkg toolchain auto-pickup
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain file")
endif()

project(Continuous
    VERSION 0.1.0
    LANGUAGES CXX C
    DESCRIPTION "Continuous game engine"
)

# ----------------------------------------------------------------------------
# Global settings
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /permissive- /Zc:__cplusplus /Zc:preprocessor /MP /utf-8 /EHsc /bigobj)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS UNICODE _UNICODE)
    # Multi-config builds get debug info per config.
    add_compile_options($<$<CONFIG:Debug>:/Zi>)
    add_link_options($<$<CONFIG:Debug>:/DEBUG>)
endif()

# Default to Release if user did not specify (single-config generators)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Output dirs - everything next to each other for easy DLL discovery
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Platform guard - this engine targets Windows only.
if(NOT WIN32)
    message(FATAL_ERROR "Continuous currently targets Windows only.")
endif()

# Helper: copy a runtime asset directory to the build output.
function(continuous_copy_assets TARGET SRC DST)
    add_custom_command(TARGET ${TARGET} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${SRC}" "$<TARGET_FILE_DIR:${TARGET}>/${DST}"
        VERBATIM
    )
endfunction()

# ----------------------------------------------------------------------------
# Subprojects
# ----------------------------------------------------------------------------
add_subdirectory(engine)
add_subdirectory(tools/cooker)
add_subdirectory(editor)
add_subdirectory(runtime)
add_subdirectory(sandbox)

# ----------------------------------------------------------------------------
# IDE folders
# ----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
