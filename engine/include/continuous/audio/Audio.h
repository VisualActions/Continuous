// Audio - miniaudio-backed engine.
// Buses: master / sfx / music / ui. Sources can be 3D-spatialized or 2D.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/math/Math.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace cn::audio {

enum class Bus : u32 { Master = 0, SFX, Music, UI, Count };

struct CN_API ListenerState {
    math::vec3 position {0, 0, 0};
    math::vec3 forward  {0, 0, 1};
    math::vec3 up       {0, 1, 0};
    math::vec3 velocity {0, 0, 0};
};

class CN_API System {
public:
    System();
    ~System();
    CN_NONCOPYABLE(System);

    bool init();
    void shutdown();

    void update(f32 dt);

    // Volume per bus (0..1+).
    void set_bus_volume(Bus b, f32 v);
    f32  bus_volume    (Bus b) const;

    // Listener state - usually set every frame from the main camera.
    void set_listener(const ListenerState& l);

    // Sounds.
    // Loaded by id - the asset manager exposes the cooked clip path; we feed
    // miniaudio that path. For uncooked WAV/OGG/MP3 we fall back to the raw
    // path lookup.
    void* play_clip(const std::string& clip_id, Bus bus = Bus::SFX,
                    f32 volume = 1.0f, f32 pitch = 1.0f, bool loop = false);
    void* play_clip_3d(const std::string& clip_id, math::vec3 position,
                       Bus bus = Bus::SFX, f32 min_dist = 1.0f, f32 max_dist = 50.0f,
                       f32 volume = 1.0f, f32 pitch = 1.0f, bool loop = false);
    void  stop(void* handle);
    void  set_position(void* handle, math::vec3 p);
    void  set_volume  (void* handle, f32 v);
    void  set_pitch   (void* handle, f32 p);
    bool  is_playing  (void* handle) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

CN_API System& global();

} // namespace cn::audio
