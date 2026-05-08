#include "continuous/scene/Scene.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"
#include "continuous/serialize/Serialize.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace cn::scene {

using json = nlohmann::json;

ecs::Entity Scene::create_entity(std::string name) {
    ecs::Entity e = world_.create();
    auto& nc = world_.add<NameComponent>(e);
    nc.name = name.empty() ? "Entity" : std::move(name);
    world_.add<TransformComponent>(e);
    world_.add<HierarchyComponent>(e);
    return e;
}

void Scene::destroy_entity(ecs::Entity e) {
    if (!world_.alive(e)) return;
    // Detach from hierarchy first.
    auto* h = world_.get<HierarchyComponent>(e);
    if (h) {
        // Remove from parent's child list.
        if (h->parent.valid()) {
            auto* ph = world_.get<HierarchyComponent>(h->parent);
            if (ph) {
                if (ph->first_child == e) ph->first_child = h->next_sibling;
            }
        }
        if (h->prev_sibling.valid()) {
            auto* ps = world_.get<HierarchyComponent>(h->prev_sibling);
            if (ps) ps->next_sibling = h->next_sibling;
        }
        if (h->next_sibling.valid()) {
            auto* ns = world_.get<HierarchyComponent>(h->next_sibling);
            if (ns) ns->prev_sibling = h->prev_sibling;
        }
        // Destroy children recursively.
        std::vector<ecs::Entity> children;
        ecs::Entity c = h->first_child;
        while (c.valid()) {
            children.push_back(c);
            auto* ch = world_.get<HierarchyComponent>(c);
            c = ch ? ch->next_sibling : ecs::Entity{};
        }
        for (auto& ch : children) destroy_entity(ch);
    }
    world_.destroy(e);
}

void Scene::set_parent(ecs::Entity child, ecs::Entity parent) {
    auto* h = world_.get<HierarchyComponent>(child);
    if (!h) return;
    // Detach from current parent.
    if (h->parent.valid()) {
        auto* ph = world_.get<HierarchyComponent>(h->parent);
        if (ph && ph->first_child == child) ph->first_child = h->next_sibling;
    }
    if (h->prev_sibling.valid()) {
        auto* ps = world_.get<HierarchyComponent>(h->prev_sibling);
        if (ps) ps->next_sibling = h->next_sibling;
    }
    if (h->next_sibling.valid()) {
        auto* ns = world_.get<HierarchyComponent>(h->next_sibling);
        if (ns) ns->prev_sibling = h->prev_sibling;
    }
    h->prev_sibling = {};
    h->next_sibling = {};
    h->parent       = parent;

    if (parent.valid()) {
        auto* ph = world_.get<HierarchyComponent>(parent);
        if (ph) {
            if (ph->first_child.valid()) {
                auto* fc = world_.get<HierarchyComponent>(ph->first_child);
                if (fc) fc->prev_sibling = child;
                h->next_sibling = ph->first_child;
            }
            ph->first_child = child;
        }
    }
    mark_dirty(child);
}

ecs::Entity Scene::parent_of(ecs::Entity e) const {
    const auto* h = world_.get<HierarchyComponent>(e);
    return h ? h->parent : ecs::Entity{};
}

std::vector<ecs::Entity> Scene::children_of(ecs::Entity e) const {
    std::vector<ecs::Entity> out;
    const auto* h = world_.get<HierarchyComponent>(e);
    if (!h) return out;
    ecs::Entity c = h->first_child;
    while (c.valid()) {
        out.push_back(c);
        const auto* ch = world_.get<HierarchyComponent>(c);
        c = ch ? ch->next_sibling : ecs::Entity{};
    }
    return out;
}

void Scene::mark_dirty(ecs::Entity e) {
    auto* t = world_.get<TransformComponent>(e);
    if (!t || t->dirty) return;
    t->dirty = true;
    auto* h = world_.get<HierarchyComponent>(e);
    if (!h) return;
    ecs::Entity c = h->first_child;
    while (c.valid()) {
        mark_dirty(c);
        const auto* ch = world_.get<HierarchyComponent>(c);
        c = ch ? ch->next_sibling : ecs::Entity{};
    }
}

void Scene::propagate_(ecs::Entity e, const math::mat4& parent_world) {
    auto* t = world_.get<TransformComponent>(e);
    if (!t) return;
    if (t->dirty) {
        t->world = parent_world * t->local.to_matrix();
        t->dirty = false;
    }
    const auto* h = world_.get<HierarchyComponent>(e);
    if (!h) return;
    ecs::Entity c = h->first_child;
    while (c.valid()) {
        propagate_(c, t->world);
        const auto* ch = world_.get<HierarchyComponent>(c);
        c = ch ? ch->next_sibling : ecs::Entity{};
    }
}

void Scene::update_world_transforms() {
    // Walk every root and propagate.
    world_.each<TransformComponent, HierarchyComponent>(
        [&](ecs::Entity e, TransformComponent&, HierarchyComponent& h) {
            if (!h.parent.valid()) propagate_(e, math::mat4(1.0f));
        });
}

void Scene::each_entity(std::function<void(ecs::Entity)> fn) const {
    world_.each_entity(fn);
}

void Scene::clear() {
    std::vector<ecs::Entity> all;
    world_.each_entity([&](ecs::Entity e){ all.push_back(e); });
    for (auto e : all) world_.destroy(e);
}

// ----------------------------------------------------------------------------
// Serialization (JSON). The cooked binary path uses serialize::write_binary
// per-component for stability across schema versions.
// ----------------------------------------------------------------------------
template <typename T>
static void save_component_if_present(const ecs::World& world, ecs::Entity e, json& dst, const char* key) {
    if (auto* c = world.get<T>(e)) {
        dst[key] = serialize::to_json(*c);
    }
}

template <typename T>
static void load_component_if_present(ecs::World& world, ecs::Entity e, const json& src, const char* key) {
    auto it = src.find(key);
    if (it == src.end()) return;
    if (!world.has<T>(e)) world.add<T>(e);
    if (auto* c = world.get<T>(e)) serialize::from_json(*it, *c);
}

bool Scene::save_json(const std::filesystem::path& p) const {
    json root;
    root["version"] = 1;
    root["entities"] = json::array();

    // We need a stable id mapping (Entity -> u32 index in array).
    std::vector<ecs::Entity> ents;
    world_.each_entity([&](ecs::Entity e) { ents.push_back(e); });
    std::unordered_map<u64, u32> id_map;
    for (u32 i = 0; i < ents.size(); ++i) id_map[handle_hash(ents[i])] = i;

    for (auto e : ents) {
        json ej;
        ej["id"] = id_map[handle_hash(e)];

        if (auto* h = world_.get<HierarchyComponent>(e)) {
            if (h->parent.valid()) {
                auto it = id_map.find(handle_hash(h->parent));
                if (it != id_map.end()) ej["parent"] = it->second;
            }
        }

        save_component_if_present<NameComponent>            (world_, e, ej, "name");
        save_component_if_present<TransformComponent>       (world_, e, ej, "transform");
        save_component_if_present<CameraComponent>          (world_, e, ej, "camera");
        save_component_if_present<LightComponent>           (world_, e, ej, "light");
        save_component_if_present<MeshRenderer>             (world_, e, ej, "mesh_renderer");
        save_component_if_present<RigidBodyComponent>       (world_, e, ej, "rigid_body");
        save_component_if_present<CharacterControllerComponent>(world_, e, ej, "character");
        save_component_if_present<AudioSourceComponent>     (world_, e, ej, "audio_source");
        save_component_if_present<AudioListenerComponent>   (world_, e, ej, "audio_listener");
        save_component_if_present<NetReplicateComponent>    (world_, e, ej, "net");
        save_component_if_present<ScriptComponent>          (world_, e, ej, "script");

        // Mesh renderer materials need explicit list serialization (vector<string>).
        if (auto* mr = world_.get<MeshRenderer>(e)) {
            ej["mesh_renderer"]["mesh_id"] = mr->mesh_id;
            ej["mesh_renderer"]["materials"] = mr->material_ids;
        }

        root["entities"].push_back(ej);
    }
    return io::write_file_text(p, root.dump(2));
}

bool Scene::load_json(const std::filesystem::path& p) {
    auto src = io::read_file_text(p);
    if (!src) return false;
    json root;
    try { root = json::parse(*src); }
    catch (std::exception& ex) {
        CN_ERROR("scene", "load_json parse failed: {}", ex.what());
        return false;
    }

    clear();

    // First pass: create entities, remember mapping.
    std::vector<ecs::Entity> created;
    std::unordered_map<u32, ecs::Entity> by_id;
    if (!root.contains("entities")) return true;
    for (auto& ej : root["entities"]) {
        ecs::Entity e = create_entity();
        u32 id = ej.value("id", 0u);
        by_id[id] = e;
        created.push_back(e);
    }
    // Second pass: load components & parenting.
    u32 i = 0;
    for (auto& ej : root["entities"]) {
        ecs::Entity e = created[i++];
        load_component_if_present<NameComponent>            (world_, e, ej, "name");
        load_component_if_present<TransformComponent>       (world_, e, ej, "transform");
        load_component_if_present<CameraComponent>          (world_, e, ej, "camera");
        load_component_if_present<LightComponent>           (world_, e, ej, "light");
        load_component_if_present<MeshRenderer>             (world_, e, ej, "mesh_renderer");
        load_component_if_present<RigidBodyComponent>       (world_, e, ej, "rigid_body");
        load_component_if_present<CharacterControllerComponent>(world_, e, ej, "character");
        load_component_if_present<AudioSourceComponent>     (world_, e, ej, "audio_source");
        load_component_if_present<AudioListenerComponent>   (world_, e, ej, "audio_listener");
        load_component_if_present<NetReplicateComponent>    (world_, e, ej, "net");
        load_component_if_present<ScriptComponent>          (world_, e, ej, "script");

        if (ej.contains("mesh_renderer")) {
            auto& mj = ej["mesh_renderer"];
            auto* mr = world_.get<MeshRenderer>(e);
            if (mr) {
                if (mj.contains("mesh_id"))   mr->mesh_id = mj["mesh_id"].get<std::string>();
                if (mj.contains("materials")) mr->material_ids = mj["materials"].get<std::vector<std::string>>();
            }
        }

        if (ej.contains("parent")) {
            u32 pid = ej["parent"].get<u32>();
            auto it = by_id.find(pid);
            if (it != by_id.end()) set_parent(e, it->second);
        }
    }
    update_world_transforms();
    return true;
}

bool Scene::save_binary(const std::filesystem::path& p) const {
    // Serialize JSON then write as compact binary blob (we don't need a
    // custom binary format here - the cooked path is the cooker's JSON-bin).
    json root;
    if (!save_json(p.string() + ".tmp.json")) return false;
    auto txt = io::read_file_text(p.string() + ".tmp.json");
    std::filesystem::remove(p.string() + ".tmp.json");
    if (!txt) return false;
    auto bson = json::to_msgpack(json::parse(*txt));
    return io::write_file_bytes(p, bson.data(), bson.size());
}

bool Scene::load_binary(const std::filesystem::path& p) {
    auto bytes = io::read_file_bytes(p);
    if (!bytes) return false;
    json j;
    try { j = json::from_msgpack(*bytes); }
    catch (std::exception& ex) {
        CN_ERROR("scene", "load_binary parse failed: {}", ex.what());
        return false;
    }
    auto tmp = p.string() + ".tmp.json";
    if (!io::write_file_text(tmp, j.dump())) return false;
    bool ok = load_json(tmp);
    std::filesystem::remove(tmp);
    return ok;
}

} // namespace cn::scene
