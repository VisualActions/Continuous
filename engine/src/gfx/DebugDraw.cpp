#include "continuous/gfx/DebugDraw.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Log.h"

#include <cmath>

namespace cn::gfx {

struct CBDebug {
    math::mat4 view_proj;
};

bool DebugDraw::init(Device& dev) {
    auto& sc = ShaderCompiler::get();
    auto vs = sc.compile_file("DebugDraw.hlsl", "DebugVS", ShaderStage::Vertex);
    auto ps = sc.compile_file("DebugDraw.hlsl", "DebugPS", ShaderStage::Pixel);
    if (!vs.ok || !ps.ok) return false;

    dev.d3d()->CreateVertexShader(vs.bytecode.data(), vs.bytecode.size(), nullptr, vs_.GetAddressOf());
    dev.d3d()->CreatePixelShader (ps.bytecode.data(), ps.bytecode.size(), nullptr, ps_.GetAddressOf());

    D3D11_INPUT_ELEMENT_DESC ie[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    dev.d3d()->CreateInputLayout(ie, 2, vs.bytecode.data(), vs.bytecode.size(), layout_.GetAddressOf());

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev.d3d()->CreateRasterizerState(&rd, rs_.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    dsd.StencilEnable  = FALSE;
    dev.d3d()->CreateDepthStencilState(&dsd, ds_.GetAddressOf());

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev.d3d()->CreateBlendState(&bd, bs_.GetAddressOf());

    cb_.create(dev, BufferType::Constant, sizeof(CBDebug));
    return true;
}

void DebugDraw::destroy() {
    vs_.Reset(); ps_.Reset(); layout_.Reset();
    rs_.Reset(); ds_.Reset(); bs_.Reset();
    cb_.destroy(); vbo_.destroy();
    verts_.clear(); vbo_capacity_ = 0;
}

void DebugDraw::line(math::vec3 a, math::vec3 b, math::vec4 c) {
    verts_.push_back({a, c});
    verts_.push_back({b, c});
}

void DebugDraw::aabb(const math::AABB& bx, math::vec4 c) {
    math::vec3 mn = bx.min, mx = bx.max;
    math::vec3 v[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mx.y, mn.z}, {mn.x, mx.y, mn.z},
        {mn.x, mn.y, mx.z}, {mx.x, mn.y, mx.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z}
    };
    static const u32 e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& ed : e) line(v[ed[0]], v[ed[1]], c);
}

void DebugDraw::sphere(math::vec3 ce, f32 r, math::vec4 c, u32 segs) {
    for (u32 i = 0; i < segs; ++i) {
        f32 a0 = math::kTwoPi * static_cast<f32>(i) / static_cast<f32>(segs);
        f32 a1 = math::kTwoPi * static_cast<f32>(i + 1) / static_cast<f32>(segs);
        line(ce + math::vec3(std::cos(a0)*r, 0, std::sin(a0)*r),
             ce + math::vec3(std::cos(a1)*r, 0, std::sin(a1)*r), c);
        line(ce + math::vec3(std::cos(a0)*r, std::sin(a0)*r, 0),
             ce + math::vec3(std::cos(a1)*r, std::sin(a1)*r, 0), c);
        line(ce + math::vec3(0, std::cos(a0)*r, std::sin(a0)*r),
             ce + math::vec3(0, std::cos(a1)*r, std::sin(a1)*r), c);
    }
}

void DebugDraw::axes(const math::mat4& xf, f32 length) {
    math::vec3 o = math::vec3(xf[3]);
    line(o, o + math::vec3(xf[0]) * length, math::vec4(1, 0, 0, 1));
    line(o, o + math::vec3(xf[1]) * length, math::vec4(0, 1, 0, 1));
    line(o, o + math::vec3(xf[2]) * length, math::vec4(0, 0, 1, 1));
}

void DebugDraw::grid(f32 size, u32 div, math::vec4 c) {
    f32 step = size / static_cast<f32>(div);
    f32 half = size * 0.5f;
    for (u32 i = 0; i <= div; ++i) {
        f32 x = -half + i * step;
        line({x, 0, -half}, {x, 0, half}, c);
        line({-half, 0, x}, {half, 0, x}, c);
    }
}

void DebugDraw::frustum(const math::mat4& inv_vp, math::vec4 c) {
    math::vec3 corners[8] = {
        {-1,-1, 0}, {+1,-1, 0}, {+1,+1, 0}, {-1,+1, 0},
        {-1,-1, 1}, {+1,-1, 1}, {+1,+1, 1}, {-1,+1, 1}
    };
    for (auto& cr : corners) {
        math::vec4 w = inv_vp * math::vec4(cr, 1.0f);
        cr = math::vec3(w) / w.w;
    }
    static const u32 e[12][2] = {
        {0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
    };
    for (auto& ed : e) line(corners[ed[0]], corners[ed[1]], c);
}

void DebugDraw::render(Device& dev, ID3D11RenderTargetView* rtv, ID3D11DepthStencilView* dsv,
                       u32 w, u32 h, const math::mat4& view_proj)
{
    if (verts_.empty()) return;
    auto* ctx = dev.context();

    usize need = verts_.size() * sizeof(LineV);
    if (need > vbo_capacity_) {
        vbo_.destroy();
        vbo_capacity_ = need * 2;
        vbo_.create(dev, BufferType::Vertex, vbo_capacity_, nullptr, sizeof(LineV), true);
    }
    vbo_.update(dev, verts_.data(), need);

    CBDebug d{};
    d.view_proj = view_proj;
    cb_.update(dev, &d, sizeof(d));

    ID3D11RenderTargetView* rtvs[1] = { rtv };
    ctx->OMSetRenderTargets(1, rtvs, dsv);
    D3D11_VIEWPORT vp{}; vp.Width = (FLOAT)w; vp.Height = (FLOAT)h; vp.MaxDepth = 1; ctx->RSSetViewports(1, &vp);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ctx->IASetInputLayout(layout_.Get());
    ID3D11Buffer* vb = vbo_.d3d();
    UINT stride = sizeof(LineV), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);

    ctx->RSSetState(rs_.Get());
    ctx->OMSetDepthStencilState(ds_.Get(), 0);
    float bf[4] = { 1, 1, 1, 1 };
    ctx->OMSetBlendState(bs_.Get(), bf, 0xFFFFFFFF);

    ID3D11Buffer* cbp = cb_.d3d();
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cbp);

    ctx->Draw(static_cast<UINT>(verts_.size()), 0);

    ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);
    verts_.clear();
}

} // namespace cn::gfx
