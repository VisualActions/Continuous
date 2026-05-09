# Contributing to Continuous

This is a tour of the engine's architecture so a new dev can find their way
around quickly. The code is small enough (~30–40 files) that you can read it
end-to-end in a sitting; this document is the map.

## Module layering

```
                 [editor]    [runtime]    [sandbox_gameplay.dll]
                     \          |              /
                      \         |             /
                       \--->[engine]<--------/
                              |
                  +-----+-----+-----+----+----+-----+----+
                  |     |     |     |    |    |     |    |
                core  math  reflect ecs scene gfx physics audio
                                              \              \
                                              shaders         ui  net  asset
```

Lower layers must not depend on higher ones. `core` is the only module that
can be included from anywhere; `gfx` knows about `core`+`math`+`asset`+
`reflect`; `scene` knows about `gfx`+`ecs`+`reflect`+`physics`+`audio`+`net`;
`editor` is allowed to know about everything the engine surfaces publicly.

## Where things live

| Question                                              | File                                             |
|-------------------------------------------------------|--------------------------------------------------|
| Where is the main loop?                               | `engine/src/Engine.cpp::Engine::run`             |
| How does an entity get a component?                   | `engine/include/continuous/ecs/World.h`          |
| How does the renderer queue a draw?                   | `engine/src/gfx/Renderer.cpp::geometry_pass_`    |
| Where do shadow cascades get computed?                | `engine/src/gfx/ShadowAtlas.cpp::compute_cascades` |
| How is the IBL cubemap generated?                     | `engine/src/gfx/IBL.cpp::generate_procedural`    |
| Where do gameplay DLLs get loaded?                    | `engine/src/HotReload.cpp::HotReloader::poll`    |
| How does `set_parent()` rewire the scene graph?       | `engine/src/scene/Scene.cpp::set_parent`         |
| How do reflected fields drive the inspector?          | `editor/src/Inspector.cpp::draw_field`           |
| How does the asset cooker walk the tree?              | `tools/cooker/main.cpp`                          |
| What does the package script do?                      | `scripts/package.bat`                            |

## Naming + style

- Namespaces: `cn::core`, `cn::math`, `cn::ecs`, `cn::scene`, `cn::gfx`,
  `cn::physics`, `cn::audio`, `cn::net`, `cn::ui`, `cn::asset`, `cn::reflect`,
  `cn::serialize`, `cn::platform`, `cn::gameplay`. Everything user-visible
  starts in `cn::`.
- Types: `PascalCase`. Functions: `snake_case`. Member fields: `snake_case_`
  with trailing underscore. Constants: `kCamelCase`.
- Public API: only `engine/include/continuous/**`. Implementation files in
  `engine/src/**`. Editor / runtime headers stay in their own apps.
- Macros: only `CN_*` prefix.
- Keep platform-specific code behind `#if defined(_WIN32)` and add a stub for
  the non-Windows path even though we currently only target Windows — it
  makes the intent explicit.

## Reflection

Add a new reflectable type by:

1. Declare in a header next to the type:
   ```cpp
   #include "continuous/reflect/Reflect.h"
   CN_REFLECT_DECL(::ns::MyType)
   ```
2. Implement in a .cpp:
   ```cpp
   CN_REFLECT_BEGIN(::ns::MyType)
       CN_REFLECT_FIELD(some_int, I32)
       CN_REFLECT_FIELD_RANGE(some_float, F32, 0.0f, 10.0f, 0.1f)
       CN_REFLECT_FIELD_COLOR(tint)
       CN_REFLECT_FIELD_STRUCT(transform, ::cn::math::Transform)
       CN_REFLECT_FIELD_VEC(items, ::ns::Item)
   CN_REFLECT_END(::ns::MyType)
   ```
3. The inspector and JSON/binary serializer pick it up automatically.

## Adding a new component

1. Add a struct to `scene/Components.h`.
2. Declare reflection in the same header.
3. Implement reflection in `scene/Components.cpp`.
4. Add a save/load case in `scene/Scene.cpp::save_json` /
   `load_json` if it doesn't fit through `save_component_if_present` already.
5. (Optional) drive a new system in `Engine.cpp::drive_play_systems`.

## Renderer

The frame is monolithic on purpose:

```
cull → shadow pass (per-cascade) → geometry pass (HDR) → skybox →
debug-draw lines → post (bloom + tonemap + FXAA) → present
```

Constant buffers:

- `b0` CBFrame (camera, lights, cascades) — bound globally
- `b1` CBObject (model, model_inv_t) — per-draw
- `b2` CBMaterial (PBR knobs + texture flags) — per-draw
- `b3` CBShadow (shadow VP + cascade idx) — shadow pass only

Texture slots:
- `t0` base color / `t1` normal / `t2` metallic-roughness / `t3` emissive /
  `t4` AO / `t5` irradiance / `t6` specular prefilter / `t7` BRDF LUT /
  `t8` shadow atlas / `t9` skybox

Sampler slots:
- `s0` anisotropic wrap, `s1` linear clamp, `s2` shadow comparison

## Hot reload internals

1. The engine watches `target.dll` mtime in `HotReloader::poll`.
2. On change, it copies the file to a counter-suffixed shadow path
   (`gameplay_loaded_<n>.dll`) so consecutive reloads don't fight each other.
3. It calls `snapshot_behavior_state` which serializes every live
   `Behavior*` via reflection into a JSON blob keyed by `(entity, class)`.
4. It calls `destroy_behaviors`, then `FreeLibrary` on the previous module.
5. It clears the gameplay registry, then `LoadLibrary` the new shadow copy.
6. It calls `cn_gameplay_register` (the DLL's exported entry that pulls in
   the auto-register TUs).
7. It re-instantiates behaviors and applies the snapshot back.

State that can survive a reload: anything reachable through the behavior's
reflected fields. State that cannot: raw pointers to old-module memory,
non-reflected scratch state (regenerate on `on_start`).

## Build presets

- `msvc-x64`        → Visual Studio 2022 generator, MSVC toolset
- `clang-cl-x64`    → Visual Studio 2022 generator, ClangCL toolset
- `ninja-msvc`      → Ninja Multi-Config + MSVC

All three produce identical layout under `build/<preset>/bin/<config>/`.

## Tests

There is no separate test target in this build. The `sandbox` scene is the
integration test — if it boots, the player walks, the cubes fall, the sphere
spins, the wobbler replicates, audio plays, and shadows draw, every system
is good.

## Filing changes

- One subsystem per commit. Don't lump things.
- Real commit messages. Examples in the existing history.
- Keep public headers stable where possible — gameplay DLLs are bound to
  exported symbols and ABI.
- Don't add dependencies to `vcpkg.json` without a one-line rationale.
