add_executable(continuous_editor WIN32
    src/main.cpp
    src/EditorApp.cpp
    src/EditorApp.h
    src/ImGuiBackend.cpp
    src/ImGuiBackend.h
    src/Inspector.cpp
    src/Inspector.h
    src/panels/Hierarchy.cpp
    src/panels/Hierarchy.h
    src/panels/Viewport.cpp
    src/panels/Viewport.h
    src/panels/Console.cpp
    src/panels/Console.h
    src/panels/AssetBrowser.cpp
    src/panels/AssetBrowser.h
    src/panels/Profiler.cpp
    src/panels/Profiler.h
)
target_link_libraries(continuous_editor PRIVATE continuous::engine)
target_compile_definitions(continuous_editor PRIVATE CN_EDITOR=1)
set_target_properties(continuous_editor PROPERTIES
    OUTPUT_NAME "ContinuousEditor"
    FOLDER "Apps"
)
