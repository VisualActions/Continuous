# ----------------------------------------------------------------------------
# Continuous engine static library.
# ----------------------------------------------------------------------------

# Find vcpkg packages
find_package(SDL3            CONFIG REQUIRED)
find_package(glm             CONFIG REQUIRED)
find_package(imgui           CONFIG REQUIRED)
find_package(imguizmo        CONFIG REQUIRED)
find_package(assimp          CONFIG REQUIRED)
find_package(nlohmann_json   CONFIG REQUIRED)
find_package(spdlog          CONFIG REQUIRED)
find_package(fmt             CONFIG REQUIRED)
find_package(Stb             REQUIRED)
find_package(unofficial-enet CONFIG REQUIRED)
find_package(unofficial-joltphysics CONFIG REQUIRED)
find_package(miniaudio       CONFIG REQUIRED)

# ---- Source list ----
set(ENGINE_SOURCES
    src/core/Log.cpp
    src/core/Assert.cpp
    src/core/Allocator.cpp
    src/core/Time.cpp
    src/core/Jobs.cpp
    src/core/IO.cpp

    src/math/MathReflect.cpp

    src/reflect/Reflect.cpp

    src/serialize/Serialize.cpp

    src/platform/Window.cpp
    src/platform/Input.cpp

    src/gfx/Device.cpp
    src/gfx/SwapChain.cpp
    src/gfx/Resources.cpp
    src/gfx/ResourceCache.cpp
    src/gfx/ShaderCompiler.cpp
    src/gfx/Mesh.cpp
    src/gfx/Texture.cpp
    src/gfx/Material.cpp
    src/gfx/Renderer.cpp
    src/gfx/ShadowAtlas.cpp
    src/gfx/PostProcess.cpp
    src/gfx/IBL.cpp
    src/gfx/DebugDraw.cpp

    src/ecs/World.cpp
    src/ecs/Archetype.cpp

    src/scene/Scene.cpp
    src/scene/Components.cpp

    src/asset/AssetManager.cpp
    src/asset/MeshImport.cpp
    src/asset/TextureImport.cpp
    src/asset/AudioImport.cpp

    src/physics/Physics.cpp

    src/audio/Audio.cpp

    src/net/Net.cpp

    src/ui/UI.cpp

    src/Engine.cpp
    src/HotReload.cpp
)

set(ENGINE_HEADERS
    include/continuous/core/Types.h
    include/continuous/core/Macros.h
    include/continuous/core/Log.h
    include/continuous/core/Assert.h
    include/continuous/core/Allocator.h
    include/continuous/core/String.h
    include/continuous/core/Time.h
    include/continuous/core/Jobs.h
    include/continuous/core/IO.h

    include/continuous/math/Math.h
    include/continuous/math/MathReflect.h

    include/continuous/reflect/Reflect.h
    include/continuous/serialize/Serialize.h

    include/continuous/platform/Window.h
    include/continuous/platform/Input.h

    include/continuous/gfx/Device.h
    include/continuous/gfx/SwapChain.h
    include/continuous/gfx/Resources.h
    include/continuous/gfx/ResourceCache.h
    include/continuous/gfx/ShaderCompiler.h
    include/continuous/gfx/Mesh.h
    include/continuous/gfx/Texture.h
    include/continuous/gfx/Material.h
    include/continuous/gfx/Renderer.h
    include/continuous/gfx/ShadowAtlas.h
    include/continuous/gfx/PostProcess.h
    include/continuous/gfx/IBL.h
    include/continuous/gfx/DebugDraw.h

    include/continuous/ecs/World.h
    include/continuous/ecs/Archetype.h
    include/continuous/ecs/Entity.h

    include/continuous/scene/Scene.h
    include/continuous/scene/Components.h

    include/continuous/asset/AssetManager.h
    include/continuous/asset/AssetTypes.h

    include/continuous/physics/Physics.h

    include/continuous/audio/Audio.h

    include/continuous/net/Net.h

    include/continuous/ui/UI.h

    include/continuous/Engine.h
    include/continuous/HotReload.h
)

add_library(continuous_engine STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
add_library(continuous::engine ALIAS continuous_engine)

target_compile_features(continuous_engine PUBLIC cxx_std_20)

target_include_directories(continuous_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${Stb_INCLUDE_DIR}
)

target_link_libraries(continuous_engine
    PUBLIC
        SDL3::SDL3
        glm::glm
        imgui::imgui
        imguizmo::imguizmo
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        fmt::fmt
    PRIVATE
        assimp::assimp
        unofficial::enet::enet
        unofficial::joltphysics::Jolt
        miniaudio::miniaudio
        d3d11
        dxgi
        d3dcompiler
        dxguid
        winmm
        ws2_32
)

target_compile_definitions(continuous_engine
    PUBLIC
        CN_PLATFORM_WINDOWS=1
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
        GLM_FORCE_RADIANS
        GLM_ENABLE_EXPERIMENTAL
)

set_target_properties(continuous_engine PROPERTIES FOLDER "Engine")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX "Header Files" FILES ${ENGINE_HEADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src     PREFIX "Source Files" FILES ${ENGINE_SOURCES})
