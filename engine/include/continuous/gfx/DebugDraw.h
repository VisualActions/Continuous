// Immediate-mode debug line drawer. Useful for physics colliders, gizmos in
// the editor, navigation paths, etc.
#pragma once

#include "continuous/gfx/Resources.h"
#include "continuous/math/Math.h"

namespace cn::gfx {

class CN_API DebugDraw {
public:
    DebugDraw() = default;
    ~DebugDraw() = default;
    CN_NONCOPYABLE(DebugDraw);

    bool init(Device& dev);
    void destroy();

    void line(math::vec3 a, math::vec3 b, math::vec4 color = math::vec4(1, 1, 0, 1));
    void aabb(const math::AABB& box, math::vec4 color = math::vec4(0, 1, 1, 1));
    void sphere(math::vec3 c, f32 r, math::vec4 color = math::vec4(1, 0, 1, 1), u32 segments = 24);
    void axes(const math::mat4& xform, f32 length = 1.0f);
    void grid(f32 size = 10.0f, u32 div = 10, math::vec4 color = math::vec4(0.4f, 0.4f, 0.4f, 1.0f));
    void frustum(const math::mat4& inv_vp, math::vec4 color = math::vec4(1, 1, 1, 1));

    void render(Device& dev, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv,
                u32 w, u32 h, const math::mat4& view_proj);

    void clear() { verts_.clear(); }

private:
    struct LineV { math::vec3 p; math::vec4 c; };
    std::vector<LineV>      verts_;
    Buffer                  vbo_;
    usize                   vbo_capacity_ = 0;
    Com<ID3D11VertexShader> vs_;
    Com<ID3D11PixelShader>  ps_;
    Com<ID3D11InputLayout>  layout_;
    Com<ID3D11RasterizerState> rs_;
    Com<ID3D11DepthStencilState> ds_;
    Com<ID3D11BlendState>      bs_;
    Buffer                  cb_;
};

} // namespace cn::gfx
