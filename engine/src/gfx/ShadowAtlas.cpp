#include "continuous/gfx/ShadowAtlas.h"
#include "continuous/core/Log.h"

#include <algorithm>

namespace cn::gfx {

bool ShadowAtlas::init(Device& dev, u32 cascade_size) {
    size_ = cascade_size;
    D3D11_TEXTURE2D_DESC d{};
    d.Width  = size_;
    d.Height = size_;
    d.MipLevels = 1;
    d.ArraySize = kShadowCascades;
    d.Format = DXGI_FORMAT_R32_TYPELESS;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    if (FAILED(dev.d3d()->CreateTexture2D(&d, nullptr, atlas_.GetAddressOf()))) {
        CN_ERROR("gfx", "shadow atlas CreateTexture2D failed");
        return false;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
    sv.Format = DXGI_FORMAT_R32_FLOAT;
    sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    sv.Texture2DArray.MipLevels = 1;
    sv.Texture2DArray.ArraySize = kShadowCascades;
    dev.d3d()->CreateShaderResourceView(atlas_.Get(), &sv, atlas_srv_.GetAddressOf());

    for (u32 i = 0; i < kShadowCascades; ++i) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = DXGI_FORMAT_D32_FLOAT;
        dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
        dv.Texture2DArray.ArraySize = 1;
        dv.Texture2DArray.FirstArraySlice = i;
        dev.d3d()->CreateDepthStencilView(atlas_.Get(), &dv, dsv_[i].GetAddressOf());
    }
    return true;
}

void ShadowAtlas::destroy() {
    for (auto& d : dsv_) d.Reset();
    atlas_srv_.Reset();
    atlas_.Reset();
}

void ShadowAtlas::compute_cascades(const math::mat4& view, const math::mat4& proj,
                                   f32 cam_near, f32 cam_far, math::vec3 light_dir)
{
    // Practical Split Scheme: lerp between log + uniform.
    f32 lambda = 0.65f;
    f32 ratio = cam_far / cam_near;
    f32 splits[kShadowCascades + 1];
    splits[0] = cam_near;
    for (u32 i = 1; i < kShadowCascades; ++i) {
        f32 p = static_cast<f32>(i) / static_cast<f32>(kShadowCascades);
        f32 log_  = cam_near * std::pow(ratio, p);
        f32 unif  = cam_near + (cam_far - cam_near) * p;
        splits[i] = lambda * log_ + (1.0f - lambda) * unif;
    }
    splits[kShadowCascades] = cam_far;

    math::mat4 inv_vp = glm::inverse(proj * view);

    light_dir = glm::normalize(light_dir);
    math::vec3 up = std::abs(light_dir.y) > 0.95f ? math::vec3(0, 0, 1) : math::vec3(0, 1, 0);

    for (u32 c = 0; c < kShadowCascades; ++c) {
        f32 zn = splits[c];
        f32 zf = splits[c + 1];

        // Frustum corners in NDC
        math::vec3 corners[8] = {
            {-1,-1, 0}, {+1,-1, 0}, {+1,+1, 0}, {-1,+1, 0},
            {-1,-1, 1}, {+1,-1, 1}, {+1,+1, 1}, {-1,+1, 1}
        };
        // To world
        for (auto& cor : corners) {
            math::vec4 w = inv_vp * math::vec4(cor, 1.0f);
            cor = math::vec3(w) / w.w;
        }
        // Clip near/far to this cascade slice (linear-ish interpolation)
        // We compute slice via z proportions.
        f32 t0 = (zn - cam_near) / (cam_far - cam_near);
        f32 t1 = (zf - cam_near) / (cam_far - cam_near);
        math::vec3 slice[8];
        for (int k = 0; k < 4; ++k) {
            slice[k]     = glm::mix(corners[k], corners[k + 4], t0);
            slice[k + 4] = glm::mix(corners[k], corners[k + 4], t1);
        }

        // Frustum centre
        math::vec3 center{0};
        for (auto& v : slice) center += v;
        center *= 1.0f / 8.0f;

        // Bounding sphere (radius = max dist from center to corner)
        f32 r = 0;
        for (auto& v : slice) r = std::max(r, glm::length(v - center));
        r = std::ceil(r * 16.0f) / 16.0f; // stabilize

        math::vec3 max_e(r);
        math::vec3 min_e(-r);

        // Texel snapping for stable shadows.
        f32 texels_per_unit = static_cast<f32>(size_) / (max_e.x - min_e.x);
        math::mat4 light_view = math::look_at(center - light_dir * r, center, up);
        math::vec4 origin = light_view * math::vec4(0, 0, 0, 1);
        f32 sx = origin.x * texels_per_unit;
        f32 sy = origin.y * texels_per_unit;
        f32 rx = std::round(sx);
        f32 ry = std::round(sy);
        f32 dx = (rx - sx) / texels_per_unit;
        f32 dy = (ry - sy) / texels_per_unit;
        light_view[3][0] += dx;
        light_view[3][1] += dy;

        math::mat4 light_proj = math::ortho(min_e.x, max_e.x, min_e.y, max_e.y,
                                             -r * 6.0f, r * 6.0f);

        cascades_[c].view_proj   = light_proj * light_view;
        cascades_[c].split_depth = zf;
    }
}

} // namespace cn::gfx
