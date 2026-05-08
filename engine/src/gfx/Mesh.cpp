#include "continuous/gfx/Mesh.h"
#include "continuous/core/Log.h"

namespace cn::gfx {

bool Mesh::upload(Device& dev,
                  std::span<const Vertex> verts,
                  std::span<const u32>    indices,
                  std::span<const SubMesh> subs)
{
    if (verts.empty() || indices.empty()) return false;

    if (!vbo_.create(dev, BufferType::Vertex, verts.size_bytes(), verts.data(), sizeof(Vertex))) return false;
    if (!ibo_.create(dev, BufferType::Index,  indices.size_bytes(), indices.data(), sizeof(u32))) return false;

    vcount_ = static_cast<u32>(verts.size());
    icount_ = static_cast<u32>(indices.size());

    bounds_ = {};
    for (auto& v : verts) bounds_.expand(v.position);

    if (!subs.empty()) {
        subs_.assign(subs.begin(), subs.end());
    } else {
        SubMesh s;
        s.first_index = 0;
        s.index_count = icount_;
        s.material_id = 0;
        s.bounds = bounds_;
        subs_.push_back(s);
    }
    return true;
}

// Helpers to push a quad (4 verts, 6 indices) with given normal direction.
static void push_quad(std::vector<Vertex>& v, std::vector<u32>& i,
                      math::vec3 a, math::vec3 b, math::vec3 c, math::vec3 d,
                      math::vec3 n, math::vec3 t) {
    u32 base = static_cast<u32>(v.size());
    v.push_back({a, n, math::vec4(t, 1), {0,0}, math::vec4(1)});
    v.push_back({b, n, math::vec4(t, 1), {1,0}, math::vec4(1)});
    v.push_back({c, n, math::vec4(t, 1), {1,1}, math::vec4(1)});
    v.push_back({d, n, math::vec4(t, 1), {0,1}, math::vec4(1)});
    i.push_back(base + 0); i.push_back(base + 1); i.push_back(base + 2);
    i.push_back(base + 0); i.push_back(base + 2); i.push_back(base + 3);
}

void make_cube(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb, f32 size) {
    v.clear(); i.clear();
    f32 s = size * 0.5f;
    using glm::vec3;
    // +X
    push_quad(v, i, {+s,-s,-s},{+s,+s,-s},{+s,+s,+s},{+s,-s,+s}, {1,0,0}, {0,0,-1});
    // -X
    push_quad(v, i, {-s,-s,+s},{-s,+s,+s},{-s,+s,-s},{-s,-s,-s}, {-1,0,0},{0,0,1});
    // +Y
    push_quad(v, i, {-s,+s,-s},{-s,+s,+s},{+s,+s,+s},{+s,+s,-s}, {0,1,0}, {1,0,0});
    // -Y
    push_quad(v, i, {-s,-s,+s},{-s,-s,-s},{+s,-s,-s},{+s,-s,+s}, {0,-1,0},{-1,0,0});
    // +Z
    push_quad(v, i, {+s,-s,+s},{+s,+s,+s},{-s,+s,+s},{-s,-s,+s}, {0,0,1}, {-1,0,0});
    // -Z
    push_quad(v, i, {-s,-s,-s},{-s,+s,-s},{+s,+s,-s},{+s,-s,-s}, {0,0,-1},{1,0,0});

    aabb = {};
    for (auto& vert : v) aabb.expand(vert.position);
}

void make_sphere(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb, f32 r, u32 segs) {
    v.clear(); i.clear();
    u32 rings = segs;
    u32 sectors = segs * 2;
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = math::kPi * static_cast<f32>(ring) / static_cast<f32>(rings) - math::kHalfPi;
        f32 cp = std::cos(phi), sp = std::sin(phi);
        for (u32 s = 0; s <= sectors; ++s) {
            f32 th = math::kTwoPi * static_cast<f32>(s) / static_cast<f32>(sectors);
            f32 ct = std::cos(th), st = std::sin(th);
            math::vec3 n(cp * ct, sp, cp * st);
            math::vec3 p = n * r;
            // tangent in azimuthal direction
            math::vec3 t(-st, 0, ct);
            Vertex vert;
            vert.position = p;
            vert.normal   = n;
            vert.tangent  = math::vec4(t, 1.0f);
            vert.uv       = math::vec2(static_cast<f32>(s) / sectors, static_cast<f32>(ring) / rings);
            vert.color    = math::vec4(1);
            v.push_back(vert);
        }
    }
    for (u32 ring = 0; ring < rings; ++ring) {
        for (u32 s = 0; s < sectors; ++s) {
            u32 a = ring * (sectors + 1) + s;
            u32 b = a + (sectors + 1);
            i.push_back(a);     i.push_back(b);     i.push_back(a + 1);
            i.push_back(a + 1); i.push_back(b);     i.push_back(b + 1);
        }
    }
    aabb = { math::vec3(-r), math::vec3(r) };
}

void make_plane(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb, f32 size, u32 sub) {
    v.clear(); i.clear();
    f32 half = size * 0.5f;
    f32 step = size / static_cast<f32>(sub);
    for (u32 z = 0; z <= sub; ++z) {
        for (u32 x = 0; x <= sub; ++x) {
            Vertex vert;
            vert.position = math::vec3(-half + x * step, 0, -half + z * step);
            vert.normal   = math::vec3(0, 1, 0);
            vert.tangent  = math::vec4(1, 0, 0, 1);
            vert.uv       = math::vec2(static_cast<f32>(x) / sub, static_cast<f32>(z) / sub) * 4.0f;
            vert.color    = math::vec4(1);
            v.push_back(vert);
        }
    }
    for (u32 z = 0; z < sub; ++z) {
        for (u32 x = 0; x < sub; ++x) {
            u32 a = z * (sub + 1) + x;
            u32 b = a + (sub + 1);
            i.push_back(a);     i.push_back(b);     i.push_back(a + 1);
            i.push_back(a + 1); i.push_back(b);     i.push_back(b + 1);
        }
    }
    aabb = { math::vec3(-half, -0.001f, -half), math::vec3(half, 0.001f, half) };
}

void make_capsule(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb,
                  f32 r, f32 h, u32 segs)
{
    v.clear(); i.clear();
    f32 cyl_h = std::max(0.0f, h - 2.0f * r);
    f32 half  = cyl_h * 0.5f;
    u32 rings = segs / 2;
    // top hemisphere
    auto add_ring = [&](f32 y, f32 ring_r, math::vec3 n_y_sign, f32 tex_v) {
        for (u32 s = 0; s <= segs; ++s) {
            f32 th = math::kTwoPi * static_cast<f32>(s) / static_cast<f32>(segs);
            f32 ct = std::cos(th), st = std::sin(th);
            Vertex vert;
            vert.position = math::vec3(ct * ring_r, y, st * ring_r);
            math::vec3 n = vert.position;
            if (y > half) n.y = (y - half);
            else if (y < -half) n.y = (y + half);
            else n = math::vec3(ct, 0, st);
            n = glm::normalize(n);
            vert.normal  = n;
            vert.tangent = math::vec4(-st, 0, ct, 1);
            vert.uv      = math::vec2(static_cast<f32>(s) / segs, tex_v);
            vert.color   = math::vec4(1);
            (void)n_y_sign;
            v.push_back(vert);
        }
    };
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = math::kHalfPi * static_cast<f32>(ring) / static_cast<f32>(rings);
        f32 cp = std::cos(phi), sp = std::sin(phi);
        f32 y = half + sp * r;
        f32 ring_r = cp * r;
        add_ring(y, ring_r, math::vec3(0,1,0), 1.0f - static_cast<f32>(ring) / (2 * rings));
    }
    // cylinder middle ring
    add_ring(-half, r, math::vec3(0,0,0), 0.5f);
    // bottom hemisphere
    for (u32 ring = 0; ring <= rings; ++ring) {
        f32 phi = math::kHalfPi * static_cast<f32>(ring) / static_cast<f32>(rings);
        f32 cp = std::cos(phi), sp = std::sin(phi);
        f32 y = -half - sp * r;
        f32 ring_r = cp * r;
        add_ring(y, ring_r, math::vec3(0,-1,0), 0.5f - static_cast<f32>(ring) / (2 * rings));
    }
    // indices: connect successive rings
    u32 stride = segs + 1;
    u32 ring_count = static_cast<u32>(v.size() / stride);
    for (u32 ring = 0; ring + 1 < ring_count; ++ring) {
        for (u32 s = 0; s < segs; ++s) {
            u32 a = ring * stride + s;
            u32 b = (ring + 1) * stride + s;
            i.push_back(a);     i.push_back(b);     i.push_back(a + 1);
            i.push_back(a + 1); i.push_back(b);     i.push_back(b + 1);
        }
    }
    aabb = { math::vec3(-r, -h * 0.5f, -r), math::vec3(r, h * 0.5f, r) };
}

} // namespace cn::gfx
