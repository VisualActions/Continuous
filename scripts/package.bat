@echo off
REM ---------------------------------------------------------------------------
REM Continuous - "Package Game" pipeline.
REM
REM Usage:
REM   scripts\package.bat [BuildPreset]
REM
REM Default preset: msvc-x64 (Release config).
REM
REM Output:
REM   packaged\Continuous\
REM     ContinuousRuntime.exe
REM     sandbox_gameplay.dll
REM     SDL3.dll                (and any other transitive runtime DLLs)
REM     scene.json
REM     assets\cooked\...       (cooked asset tree)
REM     engine\shaders\...      (HLSL sources used at first launch)
REM ---------------------------------------------------------------------------
setlocal ENABLEDELAYEDEXPANSION

set ROOT=%~dp0..
set PRESET=%~1
if "%PRESET%"=="" set PRESET=msvc-x64
set CONFIG=Release
set BUILD_DIR=%ROOT%\build\%PRESET%
set OUT_DIR=%ROOT%\packaged\Continuous

if exist "%OUT_DIR%" rmdir /S /Q "%OUT_DIR%"
mkdir "%OUT_DIR%" >NUL 2>&1

REM --- 1) Configure + build (Release).
pushd "%ROOT%"
cmake --preset %PRESET% || goto :err
cmake --build --preset %PRESET%-release || goto :err
popd

REM --- 2) Cook assets if there are any in the raw tree.
if exist "%ROOT%\assets" (
    "%BUILD_DIR%\bin\%CONFIG%\cooker.exe" "%ROOT%\assets" "%ROOT%\assets\cooked"
)

REM --- 3) Copy runtime exe + gameplay DLL + transitive DLLs.
xcopy /Y "%BUILD_DIR%\bin\%CONFIG%\ContinuousRuntime.exe" "%OUT_DIR%\" >NUL
xcopy /Y "%BUILD_DIR%\bin\%CONFIG%\sandbox_gameplay.dll"  "%OUT_DIR%\" >NUL
for %%f in ("%BUILD_DIR%\bin\%CONFIG%\*.dll") do (
    xcopy /Y "%%f" "%OUT_DIR%\" >NUL
)

REM --- 4) Copy scene + cooked assets + shaders.
if exist "%ROOT%\sandbox\scenes\sandbox.json" (
    xcopy /Y "%ROOT%\sandbox\scenes\sandbox.json" "%OUT_DIR%\scene.json*" >NUL
)
if exist "%ROOT%\assets\cooked" (
    xcopy /Y /E /I "%ROOT%\assets\cooked"   "%OUT_DIR%\assets\cooked" >NUL
)
if exist "%ROOT%\engine\shaders" (
    xcopy /Y /E /I "%ROOT%\engine\shaders"  "%OUT_DIR%\engine\shaders" >NUL
)
if exist "%ROOT%\vcpkg.json" (
    xcopy /Y "%ROOT%\vcpkg.json" "%OUT_DIR%\" >NUL
)

echo.
echo ==========================================
echo   Packaged into %OUT_DIR%
echo ==========================================
exit /b 0

:err
echo packaging failed
exit /b 1
