#include "continuous/audio/Audio.h"
#include "continuous/asset/AssetManager.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#define MA_NO_DEVICE_IO
// We let miniaudio init its own default device; the comment above was wrong.
#undef MA_NO_DEVICE_IO

#include <miniaudio.h>

#include <unordered_set>
#include <vector>

namespace cn::audio {

struct System::Impl {
    ma_engine engine{};
    ma_sound_group buses[(usize)Bus::Count]{};
    f32 bus_volumes[(usize)Bus::Count] = { 1.0f, 1.0f, 1.0f, 1.0f };
    std::vector<ma_sound*> sounds;
    bool inited = false;
};

System::System() : impl_(std::make_unique<Impl>()) {}
System::~System() { shutdown(); }

bool System::init() {
    if (impl_->inited) return true;
    if (ma_engine_init(nullptr, &impl_->engine) != MA_SUCCESS) {
        CN_ERROR("audio", "ma_engine_init failed");
        return false;
    }
    for (u32 i = 0; i < (u32)Bus::Count; ++i) {
        ma_sound_group_init(&impl_->engine, 0, nullptr, &impl_->buses[i]);
        ma_sound_group_set_volume(&impl_->buses[i], impl_->bus_volumes[i]);
    }
    impl_->inited = true;
    CN_INFO("audio", "miniaudio initialized");
    return true;
}

void System::shutdown() {
    if (!impl_->inited) return;
    for (auto* s : impl_->sounds) {
        if (s) {
            ma_sound_uninit(s);
            delete s;
        }
    }
    impl_->sounds.clear();
    for (u32 i = 0; i < (u32)Bus::Count; ++i) {
        ma_sound_group_uninit(&impl_->buses[i]);
    }
    ma_engine_uninit(&impl_->engine);
    impl_->inited = false;
}

void System::update(f32 /*dt*/) {
    if (!impl_->inited) return;
    // Reap finished one-shots that were not requested as looping.
    for (auto& s : impl_->sounds) {
        if (s && !ma_sound_is_playing(s) && !ma_sound_is_looping(s)) {
            ma_sound_uninit(s);
            delete s;
            s = nullptr;
        }
    }
    impl_->sounds.erase(std::remove(impl_->sounds.begin(), impl_->sounds.end(), nullptr),
                        impl_->sounds.end());
}

void System::set_bus_volume(Bus b, f32 v) {
    if ((u32)b >= (u32)Bus::Count) return;
    impl_->bus_volumes[(u32)b] = v;
    if (impl_->inited) ma_sound_group_set_volume(&impl_->buses[(u32)b], v);
}
f32 System::bus_volume(Bus b) const {
    if ((u32)b >= (u32)Bus::Count) return 0;
    return impl_->bus_volumes[(u32)b];
}

void System::set_listener(const ListenerState& l) {
    if (!impl_->inited) return;
    ma_engine_listener_set_position (&impl_->engine, 0, l.position.x, l.position.y, l.position.z);
    ma_engine_listener_set_direction(&impl_->engine, 0, l.forward.x,  l.forward.y,  l.forward.z);
    ma_engine_listener_set_world_up (&impl_->engine, 0, l.up.x,       l.up.y,       l.up.z);
    ma_engine_listener_set_velocity (&impl_->engine, 0, l.velocity.x, l.velocity.y, l.velocity.z);
}

static std::filesystem::path resolve_clip(const std::string& id) {
    auto& mgr = asset::Manager::get();
    // Try cooked path first
    auto cooked = mgr.cooked_root() / (id + (cn::str::ends_with(id, ".caud") ? "" : ".caud"));
    if (std::filesystem::exists(cooked)) return cooked;
    auto raw = mgr.raw_root() / id;
    return raw;
}

// Cooked .caud is float32 PCM. We could decode it here, but miniaudio's
// ma_sound_init_from_file decodes ogg/wav/flac/mp3 directly; for cooked PCM we
// fall back to ma_audio_buffer.
static ma_sound* load_cooked_caud(ma_engine* eng, const std::filesystem::path& p, ma_sound_group* g) {
    auto bytes = io::read_file_bytes(p);
    if (!bytes || bytes->size() < 16) return nullptr;
    u32 magic = 0, channels = 0, sample_rate = 0, frames = 0;
    std::memcpy(&magic,       bytes->data() + 0,  4);
    std::memcpy(&channels,    bytes->data() + 4,  4);
    std::memcpy(&sample_rate, bytes->data() + 8,  4);
    std::memcpy(&frames,      bytes->data() + 12, 4);
    if (magic != 'UANC') return nullptr;
    if (bytes->size() < 16 + frames * channels * sizeof(f32)) return nullptr;
    ma_audio_buffer_config cfg = ma_audio_buffer_config_init(ma_format_f32, channels, frames,
        bytes->data() + 16, nullptr);
    auto* ab = new ma_audio_buffer{};
    if (ma_audio_buffer_init(&cfg, ab) != MA_SUCCESS) {
        delete ab;
        return nullptr;
    }
    auto* s = new ma_sound{};
    if (ma_sound_init_from_data_source(eng, ab, 0, g, s) != MA_SUCCESS) {
        ma_audio_buffer_uninit(ab);
        delete ab;
        delete s;
        return nullptr;
    }
    return s;
}

void* System::play_clip(const std::string& id, Bus bus, f32 volume, f32 pitch, bool loop) {
    if (!impl_->inited) return nullptr;
    ma_sound_group* g = &impl_->buses[(u32)bus];
    auto p = resolve_clip(id);
    auto* s = new ma_sound{};
    bool ok = false;
    if (str::ends_with(p.string(), ".caud") && std::filesystem::exists(p)) {
        delete s;
        s = load_cooked_caud(&impl_->engine, p, g);
        ok = (s != nullptr);
    } else if (std::filesystem::exists(p)) {
        ok = ma_sound_init_from_file(&impl_->engine, p.string().c_str(), 0, g, nullptr, s) == MA_SUCCESS;
    }
    if (!ok) {
        CN_WARN("audio", "could not play clip '{}'", id);
        delete s;
        return nullptr;
    }
    ma_sound_set_volume(s, volume);
    ma_sound_set_pitch(s, pitch);
    ma_sound_set_looping(s, loop);
    ma_sound_set_spatialization_enabled(s, MA_FALSE);
    ma_sound_start(s);
    impl_->sounds.push_back(s);
    return s;
}

void* System::play_clip_3d(const std::string& id, math::vec3 pos, Bus bus,
                           f32 min_dist, f32 max_dist, f32 vol, f32 pitch, bool loop) {
    auto* s = static_cast<ma_sound*>(play_clip(id, bus, vol, pitch, loop));
    if (!s) return nullptr;
    ma_sound_set_spatialization_enabled(s, MA_TRUE);
    ma_sound_set_min_distance(s, min_dist);
    ma_sound_set_max_distance(s, max_dist);
    ma_sound_set_position(s, pos.x, pos.y, pos.z);
    ma_sound_set_attenuation_model(s, ma_attenuation_model_inverse);
    return s;
}

void System::stop(void* h) { if (h) ma_sound_stop(static_cast<ma_sound*>(h)); }
void System::set_position(void* h, math::vec3 p) {
    if (h) ma_sound_set_position(static_cast<ma_sound*>(h), p.x, p.y, p.z);
}
void System::set_volume(void* h, f32 v) {
    if (h) ma_sound_set_volume(static_cast<ma_sound*>(h), v);
}
void System::set_pitch(void* h, f32 p) {
    if (h) ma_sound_set_pitch(static_cast<ma_sound*>(h), p);
}
bool System::is_playing(void* h) const {
    return h && ma_sound_is_playing(static_cast<ma_sound*>(h)) == MA_TRUE;
}

System& global() {
    static System s;
    return s;
}

} // namespace cn::audio
