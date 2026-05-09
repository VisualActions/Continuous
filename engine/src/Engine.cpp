#include "continuous/Engine.h"
#include "continuous/asset/AssetManager.h"
#include "continuous/core/Allocator.h"
#include "continuous/core/IO.h"
#include "continuous/core/Jobs.h"
#include "continuous/core/Log.h"
#include "continuous/gfx/ResourceCache.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/scene/Components.h"

namespace cn {

bool Engine::init(const EngineConfig& cfg) {
    cfg_ = cfg;

    log::init();
    CN_INFO("engine", "Continuous engine starting up");

    if (!window_.create(cfg.window)) return false;

    if (!device_.create(cfg.enable_debug_layer)) return false;
    if (!swap_.create(device_, window_.native_handle(), cfg.window.width, cfg.window.height,
                      cfg.window.vsync)) return false;

    auto& sc = gfx::ShaderCompiler::get();
    if (cfg.shaders_root.empty()) sc.set_shaders_root(io::engine_root() / "engine" / "shaders");
    else sc.set_shaders_root(cfg.shaders_root);

    if (!renderer_.init(device_, swap_)) return false;

    audio_.init();
    physics_.init();

    auto& mgr = asset::Manager::get();
    if (!cfg.cooked_root.empty()) mgr.set_cooked_root(cfg.cooked_root);
    if (!cfg.asset_root.empty())  mgr.set_raw_root(cfg.asset_root);
    mgr.init(device_);
    mgr.start_watcher();

    if (!ibl_.generate_procedural(device_, 256, 32, 5)) {
        CN_WARN("engine", "IBL generation failed; using fallback");
    }
    renderer_.set_ibl(&ibl_);

    hud_.init(device_);

    jobs::global().init();

    // Hot reload optional.
    if (!cfg.gameplay_dll.empty()) {
        hot_.init(cfg.gameplay_dll);
        gameplay::instantiate_behaviors(scene_);
    }

    play_mode_ = cfg.start_play_mode;
    inited_    = true;
    timer_     = FrameTimer{};
    return true;
}

void Engine::shutdown() {
    if (!inited_) return;

    gameplay::destroy_behaviors(scene_);
    hot_.shutdown();

    jobs::global().shutdown();
    hud_.shutdown();

    renderer_.set_ibl(nullptr);
    ibl_.destroy();

    asset::Manager::get().shutdown();
    gfx::ResourceCache::get().clear();

    physics_.shutdown();
    audio_.shutdown();

    renderer_.shutdown();
    swap_.destroy();
    device_.destroy();
    window_.destroy();

    mem::dump_leaks();
    log::shutdown();
    inited_ = false;
}

void Engine::set_play_mode(bool v) {
    if (play_mode_ == v) return;
    play_mode_ = v;
    if (play_mode_) {
        gameplay::destroy_behaviors(scene_);
        gameplay::instantiate_behaviors(scene_);
        scene_.world().each<scene::AudioSourceComponent, scene::TransformComponent>(
            [&](ecs::Entity, scene::AudioSourceComponent& a, scene::TransformComponent&) {
                if (a.play_on_start && !a.clip_id.empty()) {
                    a.native = audio_.play_clip(a.clip_id, audio::Bus::SFX, a.volume, a.pitch, a.loop);
                }
            });
    } else {
        gameplay::destroy_behaviors(scene_);
        scene_.world().each<scene::AudioSourceComponent>([&](ecs::Entity, scene::AudioSourceComponent& a) {
            if (a.native) audio_.stop(a.native);
            a.native = nullptr;
        });
    }
}

void Engine::resolve_renderable_assets_() {
    auto& mgr = asset::Manager::get();
    scene_.world().each<scene::MeshRenderer>([&](ecs::Entity, scene::MeshRenderer& mr) {
        if (!mr.mesh_id.empty() && !mr.mesh) mr.mesh = mgr.load_mesh(mr.mesh_id);
        if (mr.materials.size() < mr.material_ids.size())
            mr.materials.resize(mr.material_ids.size(), nullptr);
        for (usize i = 0; i < mr.material_ids.size(); ++i) {
            if (!mr.materials[i] && !mr.material_ids[i].empty())
                mr.materials[i] = mgr.load_material(mr.material_ids[i]);
        }
    });
}

void Engine::apply_camera_() {
    // Find the primary camera.
    gfx::CameraData cd;
    bool found = false;
    scene_.world().each<scene::CameraComponent, scene::TransformComponent>(
        [&](ecs::Entity, scene::CameraComponent& c, scene::TransformComponent& t) {
            if (!c.primary || found) return;
            found = true;
            math::vec3 pos = math::vec3(t.world[3]);
            math::quat rot = math::quat_cast(math::mat3(t.world));
            math::vec3 fwd = rot * math::vec3(0, 0, 1);
            math::vec3 up  = rot * math::vec3(0, 1, 0);
            cd.position    = pos;
            cd.view        = math::look_at(pos, pos + fwd, up);
            cd.projection  = math::perspective(math::rad(c.fov_y_deg),
                                                window_.aspect(), c.near_z, c.far_z);
            cd.near_z      = c.near_z;
            cd.far_z       = c.far_z;
            cd.fov_y_rad   = math::rad(c.fov_y_deg);
            cd.clear_color = c.clear_color;
        });
    if (!found) {
        cd.view = math::look_at({0, 2, -8}, {0, 1, 0}, {0, 1, 0});
        cd.projection = math::perspective(math::rad(60.0f), window_.aspect(), 0.1f, 500.0f);
        cd.position = {0, 2, -8};
    }
    renderer_.set_camera(cd);
}

void Engine::drive_play_systems(f32 dt) {
    if (!play_mode_) return;

    gameplay::Context ctx;
    ctx.dt = dt;
    ctx.elapsed = static_cast<f32>(timer_.elapsed_seconds());
    ctx.window = &window_;
    gameplay::update_behaviors(scene_, ctx);

    // Physics: drive rigid bodies.
    auto& w = scene_.world();
    w.each<scene::TransformComponent, scene::RigidBodyComponent>(
        [&](ecs::Entity e, scene::TransformComponent& t, scene::RigidBodyComponent& rb) {
            if (rb.body_id == 0) {
                math::vec3 pos = t.local.position;
                math::quat rot = t.local.rotation;
                if (rb.shape == scene::ColliderShape::Box)
                    rb.body_id = physics_.add_box(e, pos, rot, rb.size, rb.mass, rb.is_static, rb.is_trigger);
                else if (rb.shape == scene::ColliderShape::Sphere)
                    rb.body_id = physics_.add_sphere(e, pos, rot, rb.size.x, rb.mass, rb.is_static, rb.is_trigger);
                else
                    rb.body_id = physics_.add_capsule(e, pos, rot, rb.size.x, rb.size.y, rb.mass, rb.is_static, rb.is_trigger);
                physics_.set_friction(rb.body_id, rb.friction);
                physics_.set_restitution(rb.body_id, rb.restitution);
                if (!rb.is_static) {
                    physics_.set_linear_velocity (rb.body_id, rb.linear_velocity);
                    physics_.set_angular_velocity(rb.body_id, rb.angular_velocity);
                }
            }
        });

    physics_.step(dt);

    w.each<scene::TransformComponent, scene::RigidBodyComponent>(
        [&](ecs::Entity, scene::TransformComponent& t, scene::RigidBodyComponent& rb) {
            if (rb.body_id == 0 || rb.is_static) return;
            math::vec3 p; math::quat q;
            physics_.get_transform(rb.body_id, p, q);
            t.local.position = p;
            t.local.rotation = q;
            t.dirty = true;
            rb.linear_velocity  = physics_.linear_velocity(rb.body_id);
            rb.angular_velocity = physics_.angular_velocity(rb.body_id);
        });

    // Character controllers.
    w.each<scene::TransformComponent, scene::CharacterControllerComponent>(
        [&](ecs::Entity e, scene::TransformComponent& t, scene::CharacterControllerComponent& cc) {
            if (!cc.native) {
                cc.native = physics_.create_character(e, t.local.position, cc.radius, cc.height);
            }
            physics_.character_step(cc.native, cc.wish_velocity, cc.wish_jump, dt);
            math::vec3 p; math::quat q;
            physics_.character_get_transform(cc.native, p, q);
            t.local.position = p;
            t.dirty = true;
            cc.grounded = physics_.character_is_grounded(cc.native);
            cc.wish_jump = false;
        });

    // Audio listener: from the primary camera.
    w.each<scene::TransformComponent, scene::AudioListenerComponent>(
        [&](ecs::Entity, scene::TransformComponent& t, scene::AudioListenerComponent& l) {
            if (!l.primary) return;
            audio::ListenerState ls;
            math::vec3 pos = math::vec3(t.world[3]);
            math::quat rot = math::quat_cast(math::mat3(t.world));
            ls.position = pos;
            ls.forward  = rot * math::vec3(0, 0, 1);
            ls.up       = rot * math::vec3(0, 1, 0);
            audio_.set_listener(ls);
        });

    // Audio source 3D positioning.
    w.each<scene::TransformComponent, scene::AudioSourceComponent>(
        [&](ecs::Entity, scene::TransformComponent& t, scene::AudioSourceComponent& a) {
            if (a.native && a.spatialize) {
                audio_.set_position(a.native, math::vec3(t.world[3]));
            }
        });

    audio_.update(dt);
    net_.update(dt);
}

void Engine::render_frame_(f32 dt) {
    (void)dt;
    scene_.update_world_transforms();
    resolve_renderable_assets_();
    apply_camera_();

    renderer_.begin_frame();

    // Submit lights.
    auto& w = scene_.world();
    w.each<scene::TransformComponent, scene::LightComponent>(
        [&](ecs::Entity, scene::TransformComponent& t, scene::LightComponent& l) {
            gfx::LightData ld;
            ld.position  = math::vec3(t.world[3]);
            math::quat rot = math::quat_cast(math::mat3(t.world));
            ld.direction = rot * math::vec3(0, 0, 1);
            ld.color     = l.color;
            ld.intensity = l.intensity;
            ld.range     = l.range;
            ld.spot_inner= math::rad(l.spot_inner_deg);
            ld.spot_outer= math::rad(l.spot_outer_deg);
            ld.casts_shadow = l.casts_shadow;
            ld.type      = static_cast<gfx::LightType>(static_cast<u32>(l.type));
            renderer_.submit_light(ld);
        });

    // Submit draws.
    w.each<scene::TransformComponent, scene::MeshRenderer>(
        [&](ecs::Entity, scene::TransformComponent& t, scene::MeshRenderer& mr) {
            if (!mr.visible || !mr.mesh) return;
            for (u32 i = 0; i < (u32)mr.mesh->subs().size(); ++i) {
                gfx::DrawItem it;
                it.mesh = mr.mesh;
                it.submesh = i;
                gfx::Material* mat = nullptr;
                u32 mid = mr.mesh->subs()[i].material_id;
                if (mid < mr.materials.size()) mat = mr.materials[mid];
                if (!mat) {
                    if (!mr.materials.empty()) mat = mr.materials[0];
                    else mat = asset::Manager::get().load_material("_default");
                }
                it.material = mat;
                it.transform = t.world;
                it.world_aabb = mr.mesh->subs()[i].bounds.transformed(t.world);
                renderer_.submit_draw(it);
            }
        });

    renderer_.render_to_swapchain();
    renderer_.end_frame();

    // HUD on top of swapchain back buffer.
    auto mp = platform::Input::get().mouse_position();
    bool md = platform::Input::get().mouse_down(platform::MouseButton::Left);
    hud_.begin_frame(swap_.width(), swap_.height(), mp, md);
    if (imgui_hook_) imgui_hook_(dt);
    // Also let the application draw HUD via post hook, but wrap pre/post.
    hud_.end_frame(device_, swap_.back_buffer_rtv());

    swap_.present();
}

int Engine::run() {
    if (!inited_) return 1;
    while (window_.process_events()) {
        f32 dt = (f32)timer_.tick();

        if (window_.was_resized()) {
            swap_.resize(window_.width(), window_.height());
            renderer_.on_resize(window_.width(), window_.height());
            window_.clear_resized();
        }

        if (hot_.loaded()) hot_.poll(scene_);
        asset::Manager::get().poll_watcher_();

        if (pre_update_) pre_update_(dt);
        drive_play_systems(dt);
        if (post_update_) post_update_(dt);

        render_frame_(dt);
    }
    return 0;
}

} // namespace cn
