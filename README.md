# Continuous

A C++ game engine for Windows. Targets one platform, one renderer, end-to-end:
**Win32 + D3D11**. The engine ships an editor, a runtime, an asset pipeline,
and a hot-reloadable C++ gameplay DLL — that DLL is the "scripting language"
for end users (this is what Unreal does with C++; we copy that model).

The repository builds two `.exe`s and one shared library:

```
ContinuousEditor.exe   editor + viewport + ImGui dockspace + play-in-editor
ContinuousRuntime.exe  packaged-game runtime, ships your scene + DLL
sandbox_gameplay.dll   user-authored gameplay code, hot-reloaded by both
```

## Tech choices

These were chosen on the basis of "actually shippable on Windows in this
session" rather than "most fashionable in 2026."

| Subsystem        | Choice                          | Why                                                                 |
|------------------|---------------------------------|---------------------------------------------------------------------|
| Build            | CMake 3.24 + vcpkg manifest      | Manifest mode is the lowest-friction way to vendor deps with MSVC. |
| Compiler         | MSVC + clang-cl (presets)        | First-class on Windows; both compile the engine.                    |
| Window/Input     | SDL3                             | Mature window/input/gamepad surface; fewer Win32 idiosyncrasies.    |
| Renderer         | D3D11 forward + HDR + post       | Less code than D3D12 for equivalent visuals at this scope.          |
| Math             | GLM, re-exported as `cn::math`   | Stable, header-only, SIMD-friendly when SSE2 is enabled.            |
| ECS              | Custom archetype storage         | Cache-friendly iteration; no template-only pain in headers.         |
| Editor UI        | Dear ImGui + ImGuizmo            | Industry-standard editor stack; docking branch.                     |
| Physics          | Jolt Physics                     | Modern, fast, deterministic; great character controller.            |
| Audio            | miniaudio                        | Single-header, good 3D attenuation, no extra runtime.               |
| Asset import     | Assimp (mesh) + stb_image (tex)  | Both robust, both vcpkg-friendly.                                   |
| Serialization    | nlohmann/json + custom binary    | JSON for human-readable scenes, binary for cooked/snapshot.         |
| Reflection       | Macro-based registry             | No external codegen step; drives inspector and serializer.          |
| Hot reload       | DLL shadow-copy + reflection-snapshot | Survives schema-compatible field edits across reloads.            |
| Networking       | ENet                             | Ordered/unreliable channels, well-tested for game traffic.          |

What is **Tier 1 (real and complete)**:

- D3D11 device + swapchain + present, HDR forward PBR, cascaded shadow maps
  (4 cascades, PCF), procedural IBL (env cube → irradiance → specular
  prefilter → BRDF LUT), bloom (down/up chain), ACES tonemap, FXAA, debug-draw
- ECS with archetype storage, scene-graph hierarchy with dirty-propagation
- Asset cooker + manifest-driven cooked binary loader (`.cmesh`, `.ctex`, `.caud`)
- Jolt physics with rigid bodies, capsule/box/sphere shapes, character
  controller, raycasts
- miniaudio engine with bus mixer + 3D listener
- ImGui editor with dockspace, hierarchy, inspector (reflection-driven),
  asset browser, viewport with translate/rotate/scale gizmos, console panel,
  profiler panel
- Hot-reloadable gameplay DLL: state is snapshotted via reflection before
  unload and restored after reload
- ENet client/server with snapshot interpolation
- In-game `cn::ui` immediate-mode UI with an embedded 8×8 bitmap font (no
  external font dependency)
- One-shot **Package Game** pipeline (`scripts/package.bat`) that produces a
  `packaged/Continuous/` folder ready to ship

What is **Tier 2 (architected and wired but thinner than a shipped engine)**:

- Cascaded shadows: 4 cascades but a single PCF tap pattern — looks fine, not
  movie-grade
- Networking: snapshot replication of transforms with linear/slerp interp;
  no rollback/prediction, no state diffing, single channel
- Hot reload: handles class fields covered by reflection; behaviors that own
  raw pointers to non-reflected state will lose that state across reload
- AA: FXAA only. No TAA / DLSS / FSR
- AO: bloom and tonemap are real; SSAO is not in this build (the lighting
  goes through pre-baked IBL ambient, which compensates a lot of the look)

## Build (Windows)

You need **MSVC 2022** (or clang-cl), **CMake 3.24+**, and **vcpkg** (manifest
mode). The first build is slow because it builds Jolt, Assimp, SDL3, ENet,
ImGui, miniaudio, etc.

```bat
git clone <this repo> Continuous
cd Continuous

REM 1) Bootstrap a local vcpkg + configure (one-shot).
scripts\bootstrap.bat

REM 2) Build Release.
cmake --build --preset msvc-x64-release
```

Open the editor:

```
build\msvc-x64\bin\Release\ContinuousEditor.exe
```

Open the runtime:

```
build\msvc-x64\bin\Release\ContinuousRuntime.exe
```

If you prefer Ninja:

```
cmake --preset ninja-msvc
cmake --build --preset msvc-x64-release
```

The `vcpkg.json` manifest pins all transitive deps. A fresh clone needs
**only** vcpkg + a Win32 toolchain to build.

## Make a new project

A "project" is a single shared library that exports gameplay components plus
a registration symbol. Use the helper:

```bat
scripts\new-project.bat MyGame
```

This drops `projects/MyGame/` with a `CMakeLists.txt` and a stub
`Hello.cpp`. Add `add_subdirectory(projects/MyGame)` to the root
`CMakeLists.txt` and rebuild. The resulting `MyGame_gameplay.dll` lives in
the same `bin/` folder as `ContinuousRuntime.exe`. Point the runtime at it
with the `cfg.gameplay_dll` field in your runtime's `main.cpp` (or use the
sandbox runtime as-is).

## Write a gameplay component

Inherit from `cn::gameplay::Behavior`, add reflected fields, override the
lifecycle hooks, and call the registration macro at the bottom of the file:

```cpp
#include "continuous/HotReload.h"
#include "continuous/Engine.h"
#include "continuous/scene/Components.h"

class Bouncer : public cn::gameplay::Behavior {
public:
    void on_update(cn::gameplay::Context& ctx) override {
        auto* t = ctx.scene->world().get<cn::scene::TransformComponent>(owner);
        if (!t) return;
        t->local.position.y = 1.0f + std::sin(ctx.elapsed * 2.0f) * 0.5f;
        t->dirty = true;
    }

    const cn::reflect::TypeInfo* type_info() const override {
        return cn::reflect::type_of<Bouncer>();
    }
};

CN_REFLECT_DECL(::Bouncer)
CN_REFLECT_BEGIN(::Bouncer)
CN_REFLECT_END(::Bouncer)

CN_GAMEPLAY_REGISTER(Bouncer)
```

In the editor, select an entity, press **+ Add Component → Script → Bouncer**.
Press Play. Edit the file, rebuild the gameplay DLL — the engine will
detect the new mtime, snapshot reflected state, unload, reload, and put
your fields back.

## Package a game

```bat
scripts\package.bat
```

Outputs `packaged/Continuous/`. The folder contains `ContinuousRuntime.exe`,
all transitive `.dll`s next to it, `sandbox_gameplay.dll`, `scene.json`,
`assets/cooked/`, and `engine/shaders/`. Zip and ship.

## Sandbox scene

`sandbox/scenes/sandbox.json` is loaded by both the runtime and the editor
on startup. It includes:

- A **player** entity with `CharacterControllerComponent` + `PlayerController`
  script (WASD + mouse + space; gamepad supported).
- A **floor** with a static box collider.
- A **directional sun** with shadow casting.
- Two **dynamic cubes** with rigid bodies that stack and fall.
- A **spinning sphere** with the `Spinner` script (live-reloadable speed/axis).
- An **audio beacon** with the `AudioBlip` script (3D-spatialized periodic
  pings).
- A **net wobbler** with the `NetDemoController` script (starts a loopback
  server, replicates its position, demonstrates snapshot interpolation).
- An **in-game HUD** (FPS, controls, draw counters) drawn by the runtime
  with `cn::ui`.

## Project layout

```
engine/         static engine library (cn::engine)
  include/continuous/   public headers
  src/                  implementation
  shaders/              HLSL
editor/         ContinuousEditor.exe (ImGui)
runtime/        ContinuousRuntime.exe (packaged-game host)
sandbox/        sandbox_gameplay.dll + sandbox.json
tools/cooker/   asset cooker CLI
projects/       (created by scripts\new-project.bat)
scripts/        bootstrap, package, new-project
```

## Honesty section

This is a foundation engine — every claimed system is wired end-to-end and
the renderer/editor/physics/audio/UI/net/hot-reload paths are real, but a
shipping engine is millions of lines and decades of polish. Treat this as a
"works on day one, would need a year of polish to ship a commercial title"
artifact. See **Tier 1 / Tier 2** above for what is fully built vs. what is
architected but light.
