#include "continuous/HotReload.h"
#include "continuous/core/Assert.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"
#include "continuous/scene/Components.h"
#include "continuous/scene/Scene.h"
#include "continuous/serialize/Serialize.h"

#include <nlohmann/json.hpp>
#include <unordered_map>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace cn::gameplay {

using json = nlohmann::json;

Registry& Registry::get() {
    static Registry r;
    return r;
}

void Registry::register_class(const std::string& name, BehaviorFactory f,
                              const reflect::TypeInfo* ti) {
    entries_[name] = Entry{ std::move(f), ti };
}

std::unique_ptr<Behavior> Registry::create(const std::string& name) const {
    auto it = entries_.find(name);
    if (it == entries_.end() || !it->second.factory) return nullptr;
    return it->second.factory();
}

const reflect::TypeInfo* Registry::type_for(const std::string& name) const {
    auto it = entries_.find(name);
    return it == entries_.end() ? nullptr : it->second.type;
}

std::vector<std::string> Registry::class_names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (auto& [k, v] : entries_) out.push_back(k);
    std::sort(out.begin(), out.end());
    return out;
}

void Registry::clear() { entries_.clear(); }

// ---------------------------------------------------------------------------
// Behavior lifecycle helpers (used by editor + runtime).
// ---------------------------------------------------------------------------
void instantiate_behaviors(scene::Scene& scene) {
    auto& w = scene.world();
    w.each<scene::ScriptComponent>([&](ecs::Entity e, scene::ScriptComponent& sc) {
        if (sc.instance) return;
        auto inst = Registry::get().create(sc.class_name);
        if (!inst) {
            CN_WARN("hotreload", "no class '{}' registered (entity {})", sc.class_name, e.idx);
            return;
        }
        inst->owner = e;
        sc.instance = inst.release();
    });
    Context ctx{};
    ctx.scene = &scene;
    w.each<scene::ScriptComponent>([&](ecs::Entity, scene::ScriptComponent& sc) {
        if (sc.instance) static_cast<Behavior*>(sc.instance)->on_start(ctx);
    });
}

void update_behaviors(scene::Scene& scene, Context& ctx) {
    ctx.scene = &scene;
    auto& w = scene.world();
    w.each<scene::ScriptComponent>([&](ecs::Entity, scene::ScriptComponent& sc) {
        if (sc.instance) static_cast<Behavior*>(sc.instance)->on_update(ctx);
    });
}

void destroy_behaviors(scene::Scene& scene) {
    Context ctx{};
    ctx.scene = &scene;
    auto& w = scene.world();
    w.each<scene::ScriptComponent>([&](ecs::Entity, scene::ScriptComponent& sc) {
        if (sc.instance) {
            static_cast<Behavior*>(sc.instance)->on_stop(ctx);
            delete static_cast<Behavior*>(sc.instance);
            sc.instance = nullptr;
        }
    });
}

std::string snapshot_behavior_state(scene::Scene& scene) {
    json root = json::array();
    auto& w = scene.world();
    w.each<scene::ScriptComponent>([&](ecs::Entity e, scene::ScriptComponent& sc) {
        if (!sc.instance) return;
        auto* ti = static_cast<Behavior*>(sc.instance)->type_info();
        if (!ti) return;
        json fields;
        serialize::to_json(fields, *ti, sc.instance);
        root.push_back({
            {"entity_idx", e.idx},
            {"entity_gen", e.generation},
            {"class", sc.class_name},
            {"fields", fields}
        });
    });
    return root.dump();
}

void restore_behavior_state(scene::Scene& scene, const std::string& snap) {
    if (snap.empty()) return;
    json root;
    try { root = json::parse(snap); } catch (...) { return; }
    auto& w = scene.world();
    for (auto& it : root) {
        u32 idx = it.value("entity_idx", 0u);
        u32 gen = it.value("entity_gen", 0u);
        std::string cls = it.value("class", std::string{});
        ecs::Entity e{ idx, gen };
        if (!w.alive(e)) continue;
        auto* sc = w.get<scene::ScriptComponent>(e);
        if (!sc || !sc->instance || sc->class_name != cls) continue;
        auto* ti = static_cast<Behavior*>(sc->instance)->type_info();
        if (!ti) continue;
        if (it.contains("fields")) serialize::from_json(it["fields"], *ti, sc->instance);
    }
}

// ---------------------------------------------------------------------------
// HotReloader.
// ---------------------------------------------------------------------------
HotReloader::~HotReloader() { shutdown(); }

bool HotReloader::init(const std::filesystem::path& target) {
    target_ = target;
    auto stem = target.stem().string();
    shadow_  = target.parent_path() / (stem + "_loaded_0.dll");
    if (!std::filesystem::exists(target_)) {
        CN_WARN("hotreload", "target dll '{}' not found", target_.string());
        return false;
    }
    last_mtime_ = std::filesystem::last_write_time(target_);
    return load_module_();
}

void HotReloader::shutdown() {
    unload_module_();
}

bool HotReloader::load_module_() {
#if defined(_WIN32)
    // Bump the shadow filename so the OS sees a "new" path each reload (prevents
    // file-locking races against open editors / debuggers caching by path).
    auto stem = target_.stem().string();
    shadow_ = target_.parent_path() / (stem + "_loaded_" + std::to_string(reload_counter_) + ".dll");
    std::error_code ec;
    std::filesystem::copy_file(target_, shadow_,
        std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        CN_ERROR("hotreload", "shadow copy failed: {}", ec.message());
        return false;
    }
    HMODULE m = LoadLibraryW(shadow_.wstring().c_str());
    if (!m) {
        CN_ERROR("hotreload", "LoadLibrary failed for {}: 0x{:08x}",
                 shadow_.string(), GetLastError());
        return false;
    }
    auto reg = reinterpret_cast<void(*)()>(GetProcAddress(m, "cn_gameplay_register"));
    if (reg) reg();
    module_ = m;
    CN_INFO("hotreload", "loaded {} ({} classes)",
            shadow_.filename().string(), Registry::get().class_names().size());
    return true;
#else
    return false;
#endif
}

void HotReloader::unload_module_() {
#if defined(_WIN32)
    if (!module_) return;
    Registry::get().clear();
    FreeLibrary(static_cast<HMODULE>(module_));
    module_ = nullptr;
#endif
}

bool HotReloader::poll(scene::Scene& scene) {
    if (target_.empty() || !std::filesystem::exists(target_)) return false;
    std::error_code ec;
    auto t = std::filesystem::last_write_time(target_, ec);
    if (ec) return false;
    if (t <= last_mtime_) return false;
    last_mtime_ = t;
    ++reload_counter_;
    CN_INFO("hotreload", "detected DLL change, reloading...");

    auto snapshot = snapshot_behavior_state(scene);
    destroy_behaviors(scene);
    unload_module_();
    if (!load_module_()) return false;
    instantiate_behaviors(scene);
    restore_behavior_state(scene, snapshot);
    return true;
}

} // namespace cn::gameplay
