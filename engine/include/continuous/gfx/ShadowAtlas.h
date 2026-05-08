// Cascaded Shadow Maps for the directional light.
#pragma once

#include "continuous/gfx/Resources.h"
#include "continuous/math/Math.h"

namespace cn::gfx {

constexpr u32 kShadowCascades = 4;

struct CN_API CascadeData {
    math::mat4 view_proj;
    f32        split_depth = 0.0f;
};

class CN_API ShadowAtlas {
public:
    ShadowAtlas() = default;
    ~ShadowAtlas() = default;
    CN_NONCOPYABLE(ShadowAtlas);

    bool init(Device& dev, u32 cascade_size = 1024);
    void destroy();

    // Compute cascade VPs from camera + directional light direction.
    void compute_cascades(const math::mat4& view, const math::mat4& proj,
                          f32 cam_near, f32 cam_far, math::vec3 light_dir);

    ID3D11DepthStencilView*   dsv(u32 cascade) const { return dsv_[cascade].Get(); }
    ID3D11ShaderResourceView* atlas_srv() const { return atlas_srv_.Get(); }
    u32  cascade_size() const { return size_; }
    const CascadeData& cascade(u32 i) const { return cascades_[i]; }

private:
    Com<ID3D11Texture2D>           atlas_;
    Com<ID3D11ShaderResourceView>  atlas_srv_;
    Com<ID3D11DepthStencilView>    dsv_[kShadowCascades];
    CascadeData                    cascades_[kShadowCascades];
    u32 size_ = 1024;
};

} // namespace cn::gfx
