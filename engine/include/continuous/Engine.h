// Continuous Engine - top-level lifecycle: brings up window/input/renderer/
// physics/audio/asset/net, runs the frame loop, integrates ImGui (in editor
// builds), drives the scene graph + ECS systems.
#pragma once

#include "continuous/HotReload.h"
#include "continuous/audio/Audio.h"
#include "continuous/core/Macros.h"
#include "continuous/core/Time.h"
#include "continuous/core/Types.h"
#include "continuous/gfx/Device.h"
#include "continuous/gfx/IBL.h"
#include "continuous/gfx/Renderer.h"
#include "continuous/gfx/SwapChain.h"
#include "continuous/net/Net.h"
#include "continuous/physics/Physics.h"
#include "continuous/platform/Window.h"
#include "continuous/scene/Scene.h"
#include "continuous/ui/UI.h"

#include <filesystem>
#include <functional>

namespace cn {

struct CN_API EngineConfig {
    platform::WindowDesc window;
    std::filesystem::path asset_root;          // raw assets dir
    std::filesystem::path cooked_root;         // cooked assets dir
    std::filesystem::path shaders_root;        // engine shaders dir
    std::filesystem::path gameplay_dll;        // hot-reload target
    bool enable_editor      = false;
    bool enable_debug_layer = false;
    bool start_play_mode    = true;            // runtime: yes; editor: no
};

class CN_API Engine {
public:
    Engine() = default;
    ~Engine() { shutdown(); }
    CN_NONCOPYABLE(Engine);

    bool init(const EngineConfig& cfg);
    void shutdown();

    // Run the main loop until window close. Returns the exit code.
    int  run();

    // Per-frame hooks the application can override.
    using FrameHook = std::function<void(f32)>;
    void set_pre_update_hook(FrameHook fn)  { pre_update_  = std::move(fn); }
    void set_post_update_hook(FrameHook fn) { post_update_ = std::move(fn); }
    void set_imgui_hook(FrameHook fn)       { imgui_hook_  = std::move(fn); }

    // Scene management.
    scene::Scene& scene() { return scene_; }

    // Subsystem accessors.
    platform::Window&  window()   { return window_; }
    gfx::Device&       device()   { return device_; }
    gfx::SwapChain&    swapchain(){ return swap_; }
    gfx::Renderer&     renderer() { return renderer_; }
    gfx::IBL&          ibl()      { return ibl_; }
    audio::System&     audio()    { return audio_; }
    physics::World&    physics()  { return physics_; }
    net::Service&      net()      { return net_; }
    ui::Context&       hud()      { return hud_; }
    FrameTimer&        timer()    { return timer_; }

    // Drive the gameplay update systems. The runtime calls this; the editor
    // calls it only while play-in-editor is active.
    void drive_play_systems(f32 dt);

    bool play_mode() const { return play_mode_; }
    void set_play_mode(bool v);

    void set_imgui_enabled(bool v) { imgui_enabled_ = v; }
    bool imgui_enabled() const { return imgui_enabled_; }

private:
    void apply_camera_();
    void render_frame_(f32 dt);
    void resolve_renderable_assets_(); // fills MeshRenderer::mesh/material from ids

    EngineConfig        cfg_;
    platform::Window    window_;
    gfx::Device         device_;
    gfx::SwapChain      swap_;
    gfx::Renderer       renderer_;
    gfx::IBL            ibl_;
    audio::System       audio_;
    physics::World      physics_;
    net::Service        net_;
    ui::Context         hud_;
    scene::Scene        scene_;
    FrameTimer          timer_;
    gameplay::HotReloader hot_;

    FrameHook           pre_update_;
    FrameHook           post_update_;
    FrameHook           imgui_hook_;

    bool inited_       = false;
    bool play_mode_    = false;
    bool imgui_enabled_ = false;
};

} // namespace cn
