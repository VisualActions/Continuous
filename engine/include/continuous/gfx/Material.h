#pragma once

#include "continuous/gfx/Texture.h"
#include "continuous/math/Math.h"

namespace cn::gfx {

struct CN_API Material {
    // PBR parameters consumed by the standard shader.
    math::vec4 base_color  {1, 1, 1, 1};
    math::vec3 emissive    {0, 0, 0};
    f32        metallic    = 0.0f;
    f32        roughness   = 1.0f;
    f32        ao          = 1.0f;
    f32        normal_strength = 1.0f;

    Texture* base_color_tex = nullptr;
    Texture* normal_tex     = nullptr;
    Texture* metallic_roughness_tex = nullptr;
    Texture* emissive_tex   = nullptr;
    Texture* ao_tex         = nullptr;

    bool double_sided = false;
    bool transparent  = false;

    std::string name;
};

} // namespace cn::gfx
