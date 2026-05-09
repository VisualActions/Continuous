# sandbox_gameplay.dll - the user's gameplay code. Hot-reloaded by the engine.
# It links the engine statically (so reflection/serialize/etc. are usable) and
# exports a single symbol "cn_gameplay_register" that the engine calls after
# LoadLibrary.
add_library(sandbox_gameplay SHARED
    src/Gameplay.cpp
    src/PlayerController.cpp
    src/PlayerController.h
    src/Spinner.cpp
    src/Spinner.h
    src/AudioBlip.cpp
    src/AudioBlip.h
    src/NetDemoController.cpp
    src/NetDemoController.h
)
target_link_libraries(sandbox_gameplay PRIVATE continuous::engine)
target_compile_definitions(sandbox_gameplay PRIVATE CN_GAMEPLAY_BUILD=1)
set_target_properties(sandbox_gameplay PROPERTIES
    OUTPUT_NAME "sandbox_gameplay"
    PREFIX ""
    FOLDER "Sandbox"
)

# Sandbox content (scene + assets) - copy alongside the editor/runtime.
set(SANDBOX_OUT "$<TARGET_FILE_DIR:sandbox_gameplay>")
add_custom_command(TARGET sandbox_gameplay POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/scenes/sandbox.json"
        "${SANDBOX_OUT}/scene.json"
)
