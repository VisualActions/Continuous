#include "continuous/asset/AssetManager.h"
#include "continuous/core/Assert.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"
#include "continuous/core/String.h"
#include "continuous/gfx/Device.h"
#include "continuous/gfx/ResourceCache.h"

#include <fstream>
#include <mutex>

namespace cn::asset {

Manager& Manager::get() {
    static Manager m;
    return m;
}

void Manager::init(gfx::Device& dev) {
    dev_ = &dev;
    // Default roots: <engine_root>/assets and <engine_root>/assets/cooked.
    auto root = io::engine_root();
    if (raw_root_.empty())    raw_root_    = root / "assets";
    if (cooked_root_.empty()) cooked_root_ = root / "assets" / "cooked";
}

void Manager::shutdown() {
    stop_watcher();
    dev_ = nullptr;
}

namespace {

// ---- cooked mesh format ----------------------------------------------------
// header:
//   magic 'CMHX'  u32
//   version       u32
//   submesh_count u32
//   vertex_count  u32
//   index_count   u32
//   bounds        2*vec3
// then for each submesh: first_index u32, index_count u32, material_id u32, AABB
// then vertices, then indices.
constexpr u32 kCMeshMagic = 'XHMC';

bool write_cooked_mesh(const std::filesystem::path& dst, const ImportedMesh& m) {
    std::filesystem::create_directories(dst.parent_path());
    std::ofstream f(dst, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    auto write = [&](const void* p, usize n) { f.write(static_cast<const char*>(p), n); };
    u32 magic = kCMeshMagic, version = 1;
    u32 sm_count = static_cast<u32>(m.submeshes.size());
    u32 v_count  = static_cast<u32>(m.vertices.size());
    u32 i_count  = static_cast<u32>(m.indices.size());
    write(&magic, 4); write(&version, 4);
    write(&sm_count, 4); write(&v_count, 4); write(&i_count, 4);
    write(&m.bounds.min, sizeof(math::vec3));
    write(&m.bounds.max, sizeof(math::vec3));
    for (auto& s : m.submeshes) {
        write(&s.first_index, 4);
        write(&s.index_count, 4);
        write(&s.material_id, 4);
        write(&s.bounds.min, sizeof(math::vec3));
        write(&s.bounds.max, sizeof(math::vec3));
    }
    write(m.vertices.data(), v_count * sizeof(gfx::Vertex));
    write(m.indices.data(),  i_count * sizeof(u32));
    return f.good();
}

bool read_cooked_mesh(const std::filesystem::path& path, ImportedMesh& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    auto read = [&](void* p, usize n) { f.read(static_cast<char*>(p), n); return static_cast<bool>(f); };
    u32 magic = 0, version = 0, sm_count = 0, v_count = 0, i_count = 0;
    if (!read(&magic, 4) || magic != kCMeshMagic) return false;
    if (!read(&version, 4)) return false;
    if (!read(&sm_count, 4)) return false;
    if (!read(&v_count, 4)) return false;
    if (!read(&i_count, 4)) return false;
    if (!read(&out.bounds.min, sizeof(math::vec3))) return false;
    if (!read(&out.bounds.max, sizeof(math::vec3))) return false;
    out.submeshes.resize(sm_count);
    for (auto& s : out.submeshes) {
        if (!read(&s.first_index, 4)) return false;
        if (!read(&s.index_count, 4)) return false;
        if (!read(&s.material_id, 4)) return false;
        if (!read(&s.bounds.min, sizeof(math::vec3))) return false;
        if (!read(&s.bounds.max, sizeof(math::vec3))) return false;
    }
    out.vertices.resize(v_count);
    out.indices.resize(i_count);
    if (v_count) read(out.vertices.data(), v_count * sizeof(gfx::Vertex));
    if (i_count) read(out.indices.data(),  i_count * sizeof(u32));
    out.ok = !out.vertices.empty();
    return out.ok;
}

// ---- cooked texture format -------------------------------------------------
// header:
//   magic 'CTEX' u32, version u32, width u32, height u32, format u32, mips u32
// then mip0 pixel data.
constexpr u32 kCTexMagic = 'XETC';

bool write_cooked_texture(const std::filesystem::path& dst, u32 w, u32 h,
                          gfx::TextureFormat fmt, const std::vector<u8>& pixels)
{
    std::filesystem::create_directories(dst.parent_path());
    std::ofstream f(dst, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    u32 magic = kCTexMagic, version = 1, mips = 1, fi = static_cast<u32>(fmt);
    f.write(reinterpret_cast<const char*>(&magic),   4);
    f.write(reinterpret_cast<const char*>(&version), 4);
    f.write(reinterpret_cast<const char*>(&w),       4);
    f.write(reinterpret_cast<const char*>(&h),       4);
    f.write(reinterpret_cast<const char*>(&fi),      4);
    f.write(reinterpret_cast<const char*>(&mips),    4);
    f.write(reinterpret_cast<const char*>(pixels.data()), pixels.size());
    return f.good();
}

bool read_cooked_texture(const std::filesystem::path& path, u32& w, u32& h,
                         gfx::TextureFormat& fmt, std::vector<u8>& pixels)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    u32 magic = 0, version = 0, mips = 0, fi = 0;
    f.read(reinterpret_cast<char*>(&magic),   4);
    if (magic != kCTexMagic) return false;
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&w),       4);
    f.read(reinterpret_cast<char*>(&h),       4);
    f.read(reinterpret_cast<char*>(&fi),      4);
    f.read(reinterpret_cast<char*>(&mips),    4);
    fmt = static_cast<gfx::TextureFormat>(fi);
    usize bytes = static_cast<usize>(w) * h * gfx::format_bpp(fmt);
    pixels.resize(bytes);
    f.read(reinterpret_cast<char*>(pixels.data()), bytes);
    return f.good() || f.eof();
}

} // namespace (cooked file io)

bool Manager::cook_mesh(const std::filesystem::path& src, const std::filesystem::path& dst) {
    auto m = import_mesh_file(src);
    if (!m.ok) return false;
    return write_cooked_mesh(dst, m);
}

bool Manager::cook_texture(const std::filesystem::path& src, const std::filesystem::path& dst) {
    std::vector<u8> px; u32 w = 0, h = 0; gfx::TextureFormat fmt;
    if (!import_texture_file(src, px, w, h, fmt)) return false;
    return write_cooked_texture(dst, w, h, fmt, px);
}

// ---- runtime loading -------------------------------------------------------
gfx::Texture* Manager::load_texture(const std::string& id) {
    if (!dev_) return nullptr;
    auto& cache = gfx::ResourceCache::get();
    if (auto* existing = cache.find_texture(id)) return existing;

    // Try cooked.
    std::filesystem::path cooked = cooked_root_ / id;
    if (str::ends_with(id, ".png") || str::ends_with(id, ".jpg") || str::ends_with(id, ".tga")) {
        cooked.replace_extension(".ctex");
    }
    if (!str::ends_with(cooked.string(), ".ctex")) cooked += ".ctex";

    std::vector<u8> px; u32 w = 0, h = 0; gfx::TextureFormat fmt = gfx::TextureFormat::SRGBA8;
    bool ok = false;
    if (std::filesystem::exists(cooked)) {
        ok = read_cooked_texture(cooked, w, h, fmt, px);
    }
    if (!ok) {
        std::filesystem::path raw = raw_root_ / id;
        if (std::filesystem::exists(raw)) ok = import_texture_file(raw, px, w, h, fmt);
    }
    if (!ok) {
        CN_WARN("asset", "texture not found: {}", id);
        return nullptr;
    }

    gfx::Texture* tex = cache.get_or_create_texture(id);
    tex->load_from_pixels(*dev_, w, h, fmt, px.data(), w * gfx::format_bpp(fmt), true);
    return tex;
}

gfx::Mesh* Manager::load_mesh(const std::string& id) {
    if (!dev_) return nullptr;
    auto& cache = gfx::ResourceCache::get();
    if (auto* existing = cache.find_mesh(id)) return existing;

    ImportedMesh m;
    bool ok = false;
    std::filesystem::path cooked = cooked_root_ / id;
    if (!str::ends_with(cooked.string(), ".cmesh")) cooked += ".cmesh";
    if (std::filesystem::exists(cooked)) ok = read_cooked_mesh(cooked, m);
    if (!ok) {
        std::filesystem::path raw = raw_root_ / id;
        if (std::filesystem::exists(raw)) m = import_mesh_file(raw); ok = m.ok;
    }

    // Procedural fallbacks if nothing was found.
    if (!ok) {
        if (id == "_procedural/cube") {
            gfx::make_cube(m.vertices, m.indices, m.bounds, 1.0f);
            ok = true;
        } else if (id == "_procedural/sphere") {
            gfx::make_sphere(m.vertices, m.indices, m.bounds, 0.5f, 32);
            ok = true;
        } else if (id == "_procedural/plane") {
            gfx::make_plane(m.vertices, m.indices, m.bounds, 20.0f, 8);
            ok = true;
        } else if (id == "_procedural/capsule") {
            gfx::make_capsule(m.vertices, m.indices, m.bounds, 0.4f, 1.8f, 24);
            ok = true;
        }
    }
    if (!ok) {
        CN_WARN("asset", "mesh not found: {}", id);
        return nullptr;
    }
    if (m.submeshes.empty()) {
        gfx::SubMesh s;
        s.first_index = 0;
        s.index_count = static_cast<u32>(m.indices.size());
        s.material_id = 0;
        s.bounds = m.bounds;
        m.submeshes.push_back(s);
    }
    gfx::Mesh* mesh = cache.get_or_create_mesh(id);
    mesh->upload(*dev_, m.vertices, m.indices, m.submeshes);
    return mesh;
}

gfx::Material* Manager::load_material(const std::string& id) {
    auto& cache = gfx::ResourceCache::get();
    return cache.get_or_create_material(id);
}

void Manager::start_watcher() {
    if (watcher_) return;
    auto* w = new io::DirectoryWatcher();
    if (w->start(raw_root_, [this](const io::ChangeEvent& ev) {
        std::lock_guard<std::mutex> lk(dirty_mu_);
        dirty_files_.push_back(ev.path);
    })) {
        watcher_ = w;
        CN_INFO("asset", "watching {}", raw_root_.string());
    } else {
        delete w;
    }
}

void Manager::stop_watcher() {
    if (!watcher_) return;
    auto* w = static_cast<io::DirectoryWatcher*>(watcher_);
    w->stop();
    delete w;
    watcher_ = nullptr;
}

void Manager::poll_watcher_() {
    std::vector<std::filesystem::path> dirty;
    {
        std::lock_guard<std::mutex> lk(dirty_mu_);
        std::swap(dirty, dirty_files_);
    }
    for (auto& p : dirty) {
        // For now, just log - a full re-cook + reload pipeline lives in the
        // cooker tool which can be triggered out-of-process.
        CN_INFO("asset", "asset changed: {}", p.string());
    }
}

} // namespace cn::asset
