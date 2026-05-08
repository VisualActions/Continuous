#include "continuous/net/Net.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <enet/enet.h>

#include <cstring>
#include <unordered_map>
#include <unordered_set>

namespace cn::net {

namespace {
struct PacketHeader { u8 kind; };

bool g_enet_inited = false;
bool ensure_enet() {
    if (g_enet_inited) return true;
    if (enet_initialize() != 0) {
        CN_ERROR("net", "enet_initialize failed");
        return false;
    }
    g_enet_inited = true;
    std::atexit([] {
        enet_deinitialize();
        g_enet_inited = false;
    });
    return true;
}
} // namespace

struct Service::Impl {
    Mode mode = Mode::Offline;
    ENetHost* host = nullptr;
    ENetPeer* server_peer = nullptr;
    std::unordered_set<ENetPeer*> clients;
    std::vector<ReplicatedSnapshot> snapshots_out;
    std::vector<ReplicatedSnapshot> snapshots_in;
    TextCallback text_cb;
    f32 send_accum = 0.0f;
    f32 send_interval = 1.0f / 20.0f;
    u32 next_net_id = 1;
    u64 bytes_in = 0;
    u64 bytes_out = 0;
};

Service::Service() : impl_(std::make_unique<Impl>()) {}
Service::~Service() { stop(); }

bool Service::start_server(u16 port, u32 max_clients) {
    if (!ensure_enet()) return false;
    stop();
    ENetAddress addr{};
    addr.host = ENET_HOST_ANY;
    addr.port = port;
    impl_->host = enet_host_create(&addr, max_clients, 2, 0, 0);
    if (!impl_->host) {
        CN_ERROR("net", "enet_host_create (server) failed on port {}", port);
        return false;
    }
    impl_->mode = Mode::Server;
    CN_INFO("net", "server listening on port {}", port);
    return true;
}

bool Service::start_client(const std::string& host, u16 port) {
    if (!ensure_enet()) return false;
    stop();
    impl_->host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!impl_->host) {
        CN_ERROR("net", "enet_host_create (client) failed");
        return false;
    }
    ENetAddress addr{};
    enet_address_set_host(&addr, host.c_str());
    addr.port = port;
    impl_->server_peer = enet_host_connect(impl_->host, &addr, 2, 0);
    if (!impl_->server_peer) {
        CN_ERROR("net", "enet_host_connect failed to {}:{}", host, port);
        enet_host_destroy(impl_->host);
        impl_->host = nullptr;
        return false;
    }
    impl_->mode = Mode::Client;
    CN_INFO("net", "client connecting to {}:{}", host, port);
    return true;
}

void Service::stop() {
    if (!impl_->host) { impl_->mode = Mode::Offline; return; }
    if (impl_->server_peer) enet_peer_disconnect(impl_->server_peer, 0);
    enet_host_flush(impl_->host);
    enet_host_destroy(impl_->host);
    impl_->host = nullptr;
    impl_->server_peer = nullptr;
    impl_->clients.clear();
    impl_->snapshots_in.clear();
    impl_->snapshots_out.clear();
    impl_->mode = Mode::Offline;
}

Mode Service::mode() const { return impl_->mode; }

static void write_snap_packet(std::vector<u8>& buf, const std::vector<ReplicatedSnapshot>& snaps, f32 t) {
    buf.clear();
    u8 kind = 0x10;
    buf.push_back(kind);
    u32 n = static_cast<u32>(snaps.size());
    buf.resize(buf.size() + 4 + 4);
    std::memcpy(buf.data() + 1, &n, 4);
    std::memcpy(buf.data() + 5, &t, 4);
    for (auto& s : snaps) {
        ReplicatedSnapshot ss = s;
        ss.time_seconds = t;
        usize off = buf.size();
        buf.resize(off + sizeof(ReplicatedSnapshot));
        std::memcpy(buf.data() + off, &ss, sizeof(ReplicatedSnapshot));
    }
}

static bool read_snap_packet(const u8* data, usize size, std::vector<ReplicatedSnapshot>& out) {
    if (size < 9 || data[0] != 0x10) return false;
    u32 n = 0; f32 t = 0;
    std::memcpy(&n, data + 1, 4);
    std::memcpy(&t, data + 5, 4);
    if (size < 9 + n * sizeof(ReplicatedSnapshot)) return false;
    out.clear();
    out.reserve(n);
    for (u32 i = 0; i < n; ++i) {
        ReplicatedSnapshot s{};
        std::memcpy(&s, data + 9 + i * sizeof(ReplicatedSnapshot), sizeof(ReplicatedSnapshot));
        s.time_seconds = t;
        out.push_back(s);
    }
    return true;
}

void Service::update(f32 dt) {
    if (!impl_->host) return;

    ENetEvent ev;
    while (enet_host_service(impl_->host, &ev, 0) > 0) {
        switch (ev.type) {
            case ENET_EVENT_TYPE_CONNECT:
                if (impl_->mode == Mode::Server) {
                    impl_->clients.insert(ev.peer);
                    CN_INFO("net", "client connected");
                    // Send a Welcome with assigned net_id.
                    u8 pkt[5];
                    pkt[0] = 0x02;
                    u32 nid = impl_->next_net_id++;
                    std::memcpy(pkt + 1, &nid, 4);
                    auto* p = enet_packet_create(pkt, sizeof(pkt), ENET_PACKET_FLAG_RELIABLE);
                    enet_peer_send(ev.peer, 0, p);
                    impl_->bytes_out += sizeof(pkt);
                } else {
                    CN_INFO("net", "connected to server");
                }
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                if (impl_->mode == Mode::Server) {
                    impl_->clients.erase(ev.peer);
                    CN_INFO("net", "client disconnected");
                } else {
                    CN_WARN("net", "disconnected from server");
                }
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                impl_->bytes_in += ev.packet->dataLength;
                const u8* d = ev.packet->data;
                usize n = ev.packet->dataLength;
                if (n >= 1) {
                    switch (d[0]) {
                        case 0x10:
                            read_snap_packet(d, n, impl_->snapshots_in);
                            break;
                        case 0x20:
                            if (impl_->text_cb && n > 1) {
                                impl_->text_cb(std::string(reinterpret_cast<const char*>(d + 1), n - 1));
                            }
                            break;
                        default: break;
                    }
                }
                enet_packet_destroy(ev.packet);
            } break;
            default: break;
        }
    }

    if (impl_->mode == Mode::Server) {
        impl_->send_accum += dt;
        if (impl_->send_accum >= impl_->send_interval && !impl_->snapshots_out.empty()) {
            impl_->send_accum = 0;
            std::vector<u8> buf;
            write_snap_packet(buf, impl_->snapshots_out,
                              static_cast<f32>(enet_time_get()) * 0.001f);
            ENetPacket* p = enet_packet_create(buf.data(), buf.size(), 0);
            enet_host_broadcast(impl_->host, 1, p);
            impl_->bytes_out += buf.size() * impl_->clients.size();
        }
    }
}

void Service::server_set_local_snapshots(const std::vector<ReplicatedSnapshot>& s) {
    impl_->snapshots_out = s;
}

const std::vector<ReplicatedSnapshot>& Service::client_latest_snapshots() const {
    return impl_->snapshots_in;
}

void Service::broadcast_text(const std::string& t) {
    if (!impl_->host) return;
    std::vector<u8> buf(1 + t.size());
    buf[0] = 0x20;
    std::memcpy(buf.data() + 1, t.data(), t.size());
    ENetPacket* p = enet_packet_create(buf.data(), buf.size(), ENET_PACKET_FLAG_RELIABLE);
    if (impl_->mode == Mode::Server) enet_host_broadcast(impl_->host, 0, p);
    else if (impl_->server_peer) enet_peer_send(impl_->server_peer, 0, p);
    impl_->bytes_out += buf.size();
}

void Service::set_text_callback(TextCallback cb) { impl_->text_cb = std::move(cb); }

u32 Service::connected_clients() const { return static_cast<u32>(impl_->clients.size()); }
u64 Service::bytes_in()  const { return impl_->bytes_in; }
u64 Service::bytes_out() const { return impl_->bytes_out; }

bool snapshot_interpolate(const ReplicatedSnapshot* snaps, u32 count, f32 t,
                          math::vec3& out_pos, math::quat& out_rot) {
    if (count == 0) return false;
    if (count == 1) {
        out_pos = snaps[0].position;
        out_rot = snaps[0].rotation;
        return true;
    }
    // Find the two surrounding snapshots.
    for (u32 i = 0; i + 1 < count; ++i) {
        if (snaps[i].time_seconds <= t && t <= snaps[i + 1].time_seconds) {
            f32 span = snaps[i + 1].time_seconds - snaps[i].time_seconds;
            f32 a = span > 1e-6f ? (t - snaps[i].time_seconds) / span : 0.0f;
            out_pos = math::lerp(snaps[i].position, snaps[i + 1].position, a);
            out_rot = math::slerp(snaps[i].rotation, snaps[i + 1].rotation, a);
            return true;
        }
    }
    out_pos = snaps[count - 1].position;
    out_rot = snaps[count - 1].rotation;
    return true;
}

Service& global() {
    static Service s;
    return s;
}

} // namespace cn::net
