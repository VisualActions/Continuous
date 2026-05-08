// Networking - ENet client/server with replication for transform components
// and snapshot interpolation on the receiving side.
//
// On-the-wire packet kinds:
//   - 0x01 Hello       (client -> server, just an introduction)
//   - 0x02 Welcome     (server -> client, you are net_id N)
//   - 0x10 Snapshot    (server -> client, batch of (net_id,pos,rot,vel))
//   - 0x11 Ack         (client -> server)
//   - 0x20 ChatLike    (free-form, used by the sandbox demo)
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/math/Math.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cn::net {

enum class Mode : u32 { Offline = 0, Server, Client };

struct CN_API ReplicatedSnapshot {
    u32         net_id = 0;
    math::vec3  position{0, 0, 0};
    math::quat  rotation{1, 0, 0, 0};
    math::vec3  velocity{0, 0, 0};
    f32         time_seconds = 0.0f;
};

class CN_API Service {
public:
    Service();
    ~Service();
    CN_NONCOPYABLE(Service);

    bool start_server(u16 port, u32 max_clients = 16);
    bool start_client(const std::string& host, u16 port);
    void stop();
    Mode mode() const;

    // Pump events. Call once per frame.
    void update(f32 dt);

    // Server: collect snapshots from local authority and broadcast.
    void server_set_local_snapshots(const std::vector<ReplicatedSnapshot>& snaps);

    // Client: get the most recent received snapshots (will be smoothed by the
    // game with interpolation).
    const std::vector<ReplicatedSnapshot>& client_latest_snapshots() const;

    // Bytes-broadcast (Sequenced reliable channel). Used by the sandbox UI.
    void broadcast_text(const std::string& text);
    using TextCallback = std::function<void(const std::string&)>;
    void set_text_callback(TextCallback cb);

    // Stats.
    u32  connected_clients() const;
    u64  bytes_in()  const;
    u64  bytes_out() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

CN_API Service& global();

// Helper: smooth an incoming snapshot ring buffer with linear+slerp interpolation
// for a target render time. Pure function, lives here so gameplay code can use
// it directly.
CN_API bool snapshot_interpolate(const ReplicatedSnapshot* snaps, u32 count,
                                 f32 render_time,
                                 math::vec3& out_pos, math::quat& out_rot);

} // namespace cn::net
