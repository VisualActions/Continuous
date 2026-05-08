// Owning resource cache for shared GPU resources keyed by string id (the
// asset's path or a synthetic name). The asset manager hands out raw pointers
// (the cache outlives every consumer for the duration of the program).
#pragma once

#include "continuous/gfx/Material.h"
#include "continuous/gfx/Mesh.h"
#include "continuous/gfx/Texture.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace cn::gfx {

class CN_API ResourceCache {
public:
    static ResourceCache& get();

    Texture*  get_or_create_texture (const std::string& id);
    Mesh*     get_or_create_mesh    (const std::string& id);
    Material* get_or_create_material(const std::string& id);

    Texture*  find_texture (const std::string& id) const;
    Mesh*     find_mesh    (const std::string& id) const;
    Material* find_material(const std::string& id) const;

    void clear();

private:
    ResourceCache() = default;
    std::unordered_map<std::string, std::unique_ptr<Texture>>  textures_;
    std::unordered_map<std::string, std::unique_ptr<Mesh>>     meshes_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;
};

} // namespace cn::gfx
