@echo off
REM Bootstrap a developer environment.
REM
REM 1) Clones vcpkg next to the project if VCPKG_ROOT is not set.
REM 2) Bootstraps it.
REM 3) Configures the msvc-x64 preset.

setlocal
set ROOT=%~dp0..

if defined VCPKG_ROOT (
    echo VCPKG_ROOT=%VCPKG_ROOT%
) else (
    if not exist "%ROOT%\..\vcpkg" (
        git clone https://github.com/microsoft/vcpkg "%ROOT%\..\vcpkg" || exit /b 1
    )
    pushd "%ROOT%\..\vcpkg"
    if not exist vcpkg.exe call bootstrap-vcpkg.bat -disableMetrics
    popd
    set "VCPKG_ROOT=%ROOT%\..\vcpkg"
    echo set VCPKG_ROOT=%VCPKG_ROOT% to persist this for new shells.
)

pushd "%ROOT%"
cmake --preset msvc-x64 || (popd & exit /b 1)
popd
echo.
echo ===================================================
echo   Open Continuous.sln in build\msvc-x64\ to build.
echo ===================================================
