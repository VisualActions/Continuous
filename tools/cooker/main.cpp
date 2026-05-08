// Continuous - asset cooker.
// Usage: cooker <input_dir> <output_dir>
//
// Walks input_dir, cooks recognized file types into output_dir, preserving
// relative path. Idempotent - skips files whose cooked counterpart is newer
// than the source.

#include "continuous/asset/AssetManager.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"
#include "continuous/core/String.h"

#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

static bool is_mesh(const fs::path& p) {
    auto e = cn::str::to_lower(p.extension().string());
    return e == ".gltf" || e == ".glb" || e == ".obj" || e == ".fbx" || e == ".dae"
        || e == ".ply"  || e == ".stl";
}
static bool is_texture(const fs::path& p) {
    auto e = cn::str::to_lower(p.extension().string());
    return e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga" || e == ".bmp" || e == ".hdr";
}
static bool is_audio(const fs::path& p) {
    auto e = cn::str::to_lower(p.extension().string());
    return e == ".wav" || e == ".ogg" || e == ".flac" || e == ".mp3";
}

static bool needs_cook(const fs::path& src, const fs::path& dst) {
    if (!fs::exists(dst)) return true;
    auto ts = fs::last_write_time(src);
    auto td = fs::last_write_time(dst);
    return ts > td;
}

int main(int argc, char** argv) {
    cn::log::init();
    cn::log::set_min_level(cn::log::Level::Info);

    if (argc < 3) {
        std::fprintf(stderr, "usage: cooker <input_dir> <output_dir>\n");
        return 1;
    }

    fs::path in  = argv[1];
    fs::path out = argv[2];
    if (!fs::exists(in)) {
        CN_ERROR("cook", "input does not exist: {}", in.string());
        return 2;
    }

    auto& mgr = cn::asset::Manager::get();
    int total = 0, cooked = 0, skipped = 0, failed = 0;

    for (auto& it : fs::recursive_directory_iterator(in)) {
        if (!it.is_regular_file()) continue;
        ++total;
        auto rel = fs::relative(it.path(), in);
        fs::path dst = out / rel;
        bool did = false;
        bool ok = true;

        if (is_mesh(rel)) {
            dst.replace_extension(".cmesh");
            if (!needs_cook(it.path(), dst)) { ++skipped; continue; }
            ok = mgr.cook_mesh(it.path(), dst);
            did = true;
        } else if (is_texture(rel)) {
            dst.replace_extension(".ctex");
            if (!needs_cook(it.path(), dst)) { ++skipped; continue; }
            ok = mgr.cook_texture(it.path(), dst);
            did = true;
        } else if (is_audio(rel)) {
            dst.replace_extension(".caud");
            if (!needs_cook(it.path(), dst)) { ++skipped; continue; }
            ok = mgr.cook_audio(it.path(), dst);
            did = true;
        } else if (rel.extension() == ".scene") {
            // Copy scene JSON unchanged.
            fs::create_directories(dst.parent_path());
            fs::copy_file(it.path(), dst, fs::copy_options::overwrite_existing);
            did = true;
        }

        if (did) {
            if (ok) { ++cooked; CN_INFO("cook", "+ {}", rel.string()); }
            else    { ++failed; CN_ERROR("cook", "! {}", rel.string()); }
        }
    }

    CN_INFO("cook", "done: {} files - cooked={} skipped={} failed={}",
            total, cooked, skipped, failed);
    cn::log::shutdown();
    return failed == 0 ? 0 : 3;
}
