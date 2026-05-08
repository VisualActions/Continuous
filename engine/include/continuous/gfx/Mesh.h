#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/gfx/Resources.h"
#include "continuous/math/Math.h"

#include <vector>

namespace cn::gfx {

struct Vertex {
    math::vec3 position;
    math::vec3 normal;
    math::vec4 tangent;       // xyz = tangent, w = bitangent sign
    math::vec2 uv;
    math::vec4 color = math::vec4(1.0f);
};

struct CN_API SubMesh {
    u32        first_index = 0;
    u32        index_count = 0;
    u32        material_id = 0; // index into the mesh's material slots
    math::AABB bounds;
};

class CN_API Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;
    CN_NONCOPYABLE(Mesh);

    bool upload(Device& dev,
                std::span<const Vertex> verts,
                std::span<const u32>    indices,
                std::span<const SubMesh> subs = {});

    Buffer& vbo() { return vbo_; }
    Buffer& ibo() { return ibo_; }
    const std::vector<SubMesh>& subs() const { return subs_; }
    u32 vertex_count() const { return vcount_; }
    u32 index_count()  const { return icount_; }
    const math::AABB& bounds() const { return bounds_; }

private:
    Buffer vbo_;
    Buffer ibo_;
    std::vector<SubMesh> subs_;
    u32  vcount_ = 0;
    u32  icount_ = 0;
    math::AABB bounds_;
};

// Generators.
CN_API void make_cube(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb,
                      f32 size = 1.0f);
CN_API void make_sphere(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb,
                        f32 radius = 0.5f, u32 segments = 32);
CN_API void make_plane(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb,
                       f32 size = 10.0f, u32 subdiv = 4);
CN_API void make_capsule(std::vector<Vertex>& v, std::vector<u32>& i, math::AABB& aabb,
                         f32 radius = 0.5f, f32 height = 1.0f, u32 segments = 24);

} // namespace cn::gfx
