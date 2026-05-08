// Continuous Engine - math types.
//
// We re-export GLM under cn::math namespace + a few engine-specific helpers
// (Transform, AABB, Frustum, Sphere, Ray, Plane). GLM is well tested,
// header-only, SIMD-friendly when SSE2 is enabled (which it is by default on
// x64 MSVC). Wrapping it gives us a stable surface; if we ever swap impls we
// only touch this header and a couple of cpps.
#pragma once

#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // D3D-style depth in [0,1]
#define GLM_FORCE_LEFT_HANDED         // D3D conventions
#define GLM_FORCE_RADIANS
#define GLM_FORCE_QUAT_DATA_WXYZ
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

#include "continuous/core/Types.h"

#include <array>
#include <limits>

namespace cn::math {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using ivec2 = glm::ivec2;
using ivec3 = glm::ivec3;
using ivec4 = glm::ivec4;
using uvec2 = glm::uvec2;
using uvec3 = glm::uvec3;
using uvec4 = glm::uvec4;
using mat3  = glm::mat3;
using mat4  = glm::mat4;
using quat  = glm::quat;

inline constexpr f32 kPi      = 3.14159265358979323846f;
inline constexpr f32 kTwoPi   = 6.28318530717958647692f;
inline constexpr f32 kHalfPi  = 1.57079632679489661923f;
inline constexpr f32 kInvPi   = 0.31830988618379067154f;
inline constexpr f32 kDegToRad= 0.01745329251994329577f;
inline constexpr f32 kRadToDeg= 57.2957795130823208768f;
inline constexpr f32 kEpsilon = 1e-5f;

CN_FORCEINLINE f32 deg(f32 r) { return r * kRadToDeg; }
CN_FORCEINLINE f32 rad(f32 d) { return d * kDegToRad; }

template <typename T>
CN_FORCEINLINE T clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

template <typename T>
CN_FORCEINLINE T lerp(T a, T b, f32 t) { return a + (b - a) * t; }

CN_FORCEINLINE vec3 lerp(vec3 a, vec3 b, f32 t) { return glm::mix(a, b, t); }
CN_FORCEINLINE quat slerp(quat a, quat b, f32 t) { return glm::slerp(a, b, t); }

CN_FORCEINLINE f32 saturate(f32 v) { return clamp(v, 0.0f, 1.0f); }

// ----------------------------------------------------------------------------
// Transform: position + rotation (quat) + uniform-or-non-uniform scale.
// ----------------------------------------------------------------------------
struct Transform {
    vec3 position{0.0f};
    quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // w,x,y,z
    vec3 scale   {1.0f};

    static Transform identity() { return {}; }

    mat4 to_matrix() const {
        mat4 m = glm::translate(mat4(1.0f), position);
        m *= glm::mat4_cast(rotation);
        m = glm::scale(m, scale);
        return m;
    }

    static Transform from_matrix(const mat4& m) {
        Transform t;
        t.position = vec3(m[3]);
        vec3 s;
        s.x = glm::length(vec3(m[0]));
        s.y = glm::length(vec3(m[1]));
        s.z = glm::length(vec3(m[2]));
        if (glm::determinant(m) < 0) s.x = -s.x;
        t.scale = s;
        mat3 rot;
        rot[0] = vec3(m[0]) / (s.x != 0 ? s.x : 1.0f);
        rot[1] = vec3(m[1]) / (s.y != 0 ? s.y : 1.0f);
        rot[2] = vec3(m[2]) / (s.z != 0 ? s.z : 1.0f);
        t.rotation = glm::quat_cast(rot);
        return t;
    }

    Transform combine(const Transform& child) const {
        Transform out;
        out.position = position + rotation * (scale * child.position);
        out.rotation = rotation * child.rotation;
        out.scale    = scale * child.scale;
        return out;
    }

    vec3 forward() const { return rotation * vec3(0,0,1); }
    vec3 right()   const { return rotation * vec3(1,0,0); }
    vec3 up()      const { return rotation * vec3(0,1,0); }
};

// ----------------------------------------------------------------------------
// AABB
// ----------------------------------------------------------------------------
struct AABB {
    vec3 min{ std::numeric_limits<f32>::infinity()};
    vec3 max{-std::numeric_limits<f32>::infinity()};

    bool valid() const { return min.x <= max.x && min.y <= max.y && min.z <= max.z; }
    vec3 center() const { return 0.5f * (min + max); }
    vec3 extent() const { return 0.5f * (max - min); }
    vec3 size()   const { return max - min; }

    void expand(vec3 p) { min = glm::min(min, p); max = glm::max(max, p); }
    void expand(const AABB& o) { min = glm::min(min, o.min); max = glm::max(max, o.max); }

    bool contains(vec3 p) const {
        return p.x >= min.x && p.y >= min.y && p.z >= min.z
            && p.x <= max.x && p.y <= max.y && p.z <= max.z;
    }

    AABB transformed(const mat4& m) const {
        // World-space AABB of an OBB transformation.
        vec3 c = center(), e = extent();
        vec3 nc = vec3(m * vec4(c, 1.0f));
        mat3 absM = mat3(
            glm::abs(vec3(m[0])),
            glm::abs(vec3(m[1])),
            glm::abs(vec3(m[2]))
        );
        vec3 ne = absM * e;
        return { nc - ne, nc + ne };
    }
};

// ----------------------------------------------------------------------------
// Sphere
// ----------------------------------------------------------------------------
struct Sphere { vec3 center{0}; f32 radius{0}; };

// ----------------------------------------------------------------------------
// Plane: ax+by+cz+d=0, n=(a,b,c) normalized.
// ----------------------------------------------------------------------------
struct Plane {
    vec3 normal{0,1,0};
    f32  d{0};
    f32 distance(vec3 p) const { return glm::dot(normal, p) + d; }
};

// ----------------------------------------------------------------------------
// Ray
// ----------------------------------------------------------------------------
struct Ray {
    vec3 origin{0};
    vec3 direction{0,0,1};
    vec3 at(f32 t) const { return origin + direction * t; }
};

// Ray vs AABB, slab method. Returns true with t entry/exit on hit.
inline bool ray_aabb(const Ray& r, const AABB& b, f32& tmin, f32& tmax) {
    tmin = 0.0f;
    tmax = std::numeric_limits<f32>::infinity();
    for (int i = 0; i < 3; ++i) {
        f32 inv = 1.0f / r.direction[i];
        f32 t1  = (b.min[i] - r.origin[i]) * inv;
        f32 t2  = (b.max[i] - r.origin[i]) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = glm::max(tmin, t1);
        tmax = glm::min(tmax, t2);
        if (tmin > tmax) return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// Frustum: 6 planes (left,right,bottom,top,near,far) extracted from VP.
// ----------------------------------------------------------------------------
struct Frustum {
    std::array<Plane, 6> planes;
    static Frustum from_view_projection(const mat4& vp);
    bool intersects(const AABB& b) const;
    bool intersects(const Sphere& s) const;
};

inline Frustum Frustum::from_view_projection(const mat4& m) {
    Frustum f;
    auto setp = [&](int i, vec4 p) {
        vec3 n(p.x, p.y, p.z);
        f32 len = glm::length(n);
        if (len > 0) { n /= len; p.w /= len; }
        f.planes[i].normal = n;
        f.planes[i].d      = p.w;
    };
    setp(0, vec4(m[0][3] + m[0][0], m[1][3] + m[1][0], m[2][3] + m[2][0], m[3][3] + m[3][0])); // left
    setp(1, vec4(m[0][3] - m[0][0], m[1][3] - m[1][0], m[2][3] - m[2][0], m[3][3] - m[3][0])); // right
    setp(2, vec4(m[0][3] + m[0][1], m[1][3] + m[1][1], m[2][3] + m[2][1], m[3][3] + m[3][1])); // bottom
    setp(3, vec4(m[0][3] - m[0][1], m[1][3] - m[1][1], m[2][3] - m[2][1], m[3][3] - m[3][1])); // top
    setp(4, vec4(m[0][2],            m[1][2],          m[2][2],          m[3][2]));            // near (D3D zero-to-one)
    setp(5, vec4(m[0][3] - m[0][2], m[1][3] - m[1][2], m[2][3] - m[2][2], m[3][3] - m[3][2])); // far
    return f;
}

inline bool Frustum::intersects(const AABB& b) const {
    for (auto& p : planes) {
        vec3 pos(
            p.normal.x >= 0 ? b.max.x : b.min.x,
            p.normal.y >= 0 ? b.max.y : b.min.y,
            p.normal.z >= 0 ? b.max.z : b.min.z
        );
        if (p.distance(pos) < 0) return false;
    }
    return true;
}

inline bool Frustum::intersects(const Sphere& s) const {
    for (auto& p : planes) if (p.distance(s.center) < -s.radius) return false;
    return true;
}

// ----------------------------------------------------------------------------
// Camera helpers.
// ----------------------------------------------------------------------------
inline mat4 perspective(f32 fovy_rad, f32 aspect, f32 near_z, f32 far_z) {
    return glm::perspectiveLH_ZO(fovy_rad, aspect, near_z, far_z);
}
inline mat4 ortho(f32 l, f32 r, f32 b, f32 t, f32 zn, f32 zf) {
    return glm::orthoLH_ZO(l, r, b, t, zn, zf);
}
inline mat4 look_at(vec3 eye, vec3 target, vec3 up) {
    return glm::lookAtLH(eye, target, up);
}

} // namespace cn::math
