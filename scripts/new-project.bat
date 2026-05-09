@echo off
REM Scaffold a new gameplay project alongside the engine.
REM
REM Usage: scripts\new-project.bat MyGame
setlocal
if "%~1"=="" (
    echo usage: new-project.bat ^<ProjectName^>
    exit /b 1
)
set NAME=%~1
set ROOT=%~dp0..\projects\%NAME%
mkdir "%ROOT%\src"     >NUL 2>&1
mkdir "%ROOT%\scenes"  >NUL 2>&1
mkdir "%ROOT%\assets"  >NUL 2>&1

> "%ROOT%\CMakeLists.txt" (
echo add_library(%NAME%_gameplay SHARED src/Hello.cpp^)
echo target_link_libraries^(%NAME%_gameplay PRIVATE continuous::engine^)
echo target_compile_definitions^(%NAME%_gameplay PRIVATE CN_GAMEPLAY_BUILD=1^)
echo set_target_properties^(%NAME%_gameplay PROPERTIES OUTPUT_NAME "%NAME%_gameplay" PREFIX "" FOLDER "Projects/%NAME%"^)
)

> "%ROOT%\src\Hello.cpp" (
echo // %NAME% - generated gameplay project. Build, drop the resulting DLL next
echo // to ContinuousEditor.exe / ContinuousRuntime.exe, and select Hello in
echo // the inspector "Add Component -^> Script" menu.
echo.
echo #include "continuous/HotReload.h"
echo #include "continuous/Engine.h"
echo #include "continuous/scene/Components.h"
echo.
echo struct Hello : cn::gameplay::Behavior {
echo     void on_update^(cn::gameplay::Context^& ctx^) override {
echo         if ^(!ctx.scene^) return;
echo         auto* t = ctx.scene-^>world^(^).get^<cn::scene::TransformComponent^>^(owner^);
echo         if ^(t^) { t-^>local.position.y += ctx.dt; t-^>dirty = true; }
echo     }
echo };
echo.
echo extern "C" CN_GAMEPLAY_API void cn_gameplay_register^(^) {}
echo CN_GAMEPLAY_REGISTER^(Hello^)
)

echo Created projects\%NAME%.
echo Add `add_subdirectory^(projects/%NAME%^)` to the root CMakeLists.txt to build it.
