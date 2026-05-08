// Asset manager - resolves asset ids ("meshes/foo.cmesh" or
// "raw:meshes/foo.gltf") to runtime gfx resources. Backed by ResourceCache.
#pragma once

#include "continuous/asset/AssetTypes.h"
#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/gfx/Material.h"
#include "continuous/gfx/Mesh.h"
#include "continuous/gfx/Texture.h"

#include <filesystem>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace cn::asset {

class CN_API Manager {
public:
    static Manager& get();

    // Roots: cooked binary first (assets/cooked/), then raw (engine_root/assets).
    void set_cooked_root(const std::filesystem::path& p) { cooked_root_ = p; }
    void set_raw_root   (const std::filesystem::path& p) { raw_root_    = p; }
    const std::filesystem::path& cooked_root() const { return cooked_root_; }
    const std::filesystem::path& raw_root()    const { return raw_root_;    }

    void init(class gfx::Device& dev);
    void shutdown();

    // Resolve / load.
    gfx::Texture*  load_texture (const std::string& id);
    gfx::Mesh*     load_mesh    (const std::string& id);
    gfx::Material* load_material(const std::string& id);

    // Import / cook helpers (also used by the cooker tool).
    bool cook_mesh   (const std::filesystem::path& src, const std::filesystem::path& dst);
    bool cook_texture(const std::filesystem::path& src, const std::filesystem::path& dst);
    bool cook_audio  (const std::filesystem::path& src, const std::filesystem::path& dst);

    // Hot reload: watches cooked + raw, reloads textures/meshes when they change.
    void start_watcher();
    void stop_watcher();
    void poll_watcher_();   // call once per frame from engine; processes pending events.

private:
    Manager() = default;
    gfx::Device* dev_ = nullptr;
    std::filesystem::path cooked_root_;
    std::filesystem::path raw_root_;
    std::vector<std::filesystem::path> dirty_files_;
    std::mutex                         dirty_mu_;
    void* watcher_ = nullptr; // io::DirectoryWatcher*
};

// Helpers exposed for the cooker tool.
struct CN_API ImportedMesh {
    std::vector<gfx::Vertex> vertices;
    std::vector<u32>         indices;
    std::vector<gfx::SubMesh> submeshes;
    math::AABB               bounds;
    bool ok = false;
};

CN_API ImportedMesh import_mesh_file(const std::filesystem::path& src);
CN_API bool import_texture_file(const std::filesystem::path& src,
                                std::vector<u8>& pixels, u32& width, u32& height,
                                gfx::TextureFormat& format);

} // namespace cn::asset
