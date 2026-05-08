#include "continuous/gfx/ResourceCache.h"

namespace cn::gfx {

ResourceCache& ResourceCache::get() {
    static ResourceCache c;
    return c;
}

Texture* ResourceCache::get_or_create_texture(const std::string& id) {
    auto it = textures_.find(id);
    if (it != textures_.end()) return it->second.get();
    auto p = std::make_unique<Texture>();
    p->set_label(id);
    Texture* raw = p.get();
    textures_.emplace(id, std::move(p));
    return raw;
}

Mesh* ResourceCache::get_or_create_mesh(const std::string& id) {
    auto it = meshes_.find(id);
    if (it != meshes_.end()) return it->second.get();
    auto p = std::make_unique<Mesh>();
    Mesh* raw = p.get();
    meshes_.emplace(id, std::move(p));
    return raw;
}

Material* ResourceCache::get_or_create_material(const std::string& id) {
    auto it = materials_.find(id);
    if (it != materials_.end()) return it->second.get();
    auto p = std::make_unique<Material>();
    p->name = id;
    Material* raw = p.get();
    materials_.emplace(id, std::move(p));
    return raw;
}

Texture* ResourceCache::find_texture(const std::string& id) const {
    auto it = textures_.find(id);
    return it == textures_.end() ? nullptr : it->second.get();
}

Mesh* ResourceCache::find_mesh(const std::string& id) const {
    auto it = meshes_.find(id);
    return it == meshes_.end() ? nullptr : it->second.get();
}

Material* ResourceCache::find_material(const std::string& id) const {
    auto it = materials_.find(id);
    return it == materials_.end() ? nullptr : it->second.get();
}

void ResourceCache::clear() {
    materials_.clear();
    meshes_.clear();
    textures_.clear();
}

} // namespace cn::gfx
