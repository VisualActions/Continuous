#include "continuous/ui/UI.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Log.h"

#include <cstring>

namespace cn::ui {

// ----------------------------------------------------------------------------
// Embedded 8x8 bitmap font - upper subset of CP437. Public-domain pattern
// commonly used in homebrew engines (e.g., the IBM PC BIOS 8x8 font). For
// space we ship printable ASCII (0x20..0x7E); other glyphs render as blank.
// ----------------------------------------------------------------------------
static const u8 kFont8x8[96][8] = {
    // 0x20 ' '
    {0,0,0,0,0,0,0,0},
    // 0x21 !
    {0x18,0x3C,0x3C,0x18,0x18,0,0x18,0},
    // "
    {0x36,0x36,0,0,0,0,0,0},
    // #
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},
    // $
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0},
    // %
    {0,0x63,0x33,0x18,0x0C,0x66,0x63,0},
    // &
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0},
    // '
    {0x06,0x06,0x03,0,0,0,0,0},
    // (
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0},
    // )
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0},
    // *
    {0,0x66,0x3C,0xFF,0x3C,0x66,0,0},
    // +
    {0,0x0C,0x0C,0x3F,0x0C,0x0C,0,0},
    // ,
    {0,0,0,0,0,0x0C,0x0C,0x06},
    // -
    {0,0,0,0x3F,0,0,0,0},
    // .
    {0,0,0,0,0,0x0C,0x0C,0},
    // /
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0},
    // 0
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0},
    // 1
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0},
    // 2
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0},
    // 3
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0},
    // 4
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0},
    // 5
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0},
    // 6
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0},
    // 7
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0},
    // 8
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0},
    // 9
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0},
    // :
    {0,0x0C,0x0C,0,0,0x0C,0x0C,0},
    // ;
    {0,0x0C,0x0C,0,0,0x0C,0x0C,0x06},
    // <
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0},
    // =
    {0,0,0x3F,0,0x3F,0,0,0},
    // >
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0},
    // ?
    {0x1E,0x33,0x30,0x18,0x0C,0,0x0C,0},
    // @
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0},
    // A
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0},
    // B
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0},
    // C
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0},
    // D
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0},
    // E
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0},
    // F
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0},
    // G
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0},
    // H
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0},
    // I
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
    // J
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0},
    // K
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0},
    // L
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0},
    // M
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0},
    // N
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0},
    // O
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0},
    // P
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0},
    // Q
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0},
    // R
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0},
    // S
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0},
    // T
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0},
    // U
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0},
    // V
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0},
    // W
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},
    // X
    {0x63,0x63,0x36,0x1C,0x36,0x63,0x63,0},
    // Y
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0},
    // Z
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0},
    // [
    {0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0},
    // backslash
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0},
    // ]
    {0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0},
    // ^
    {0x08,0x1C,0x36,0x63,0,0,0,0},
    // _
    {0,0,0,0,0,0,0,0xFF},
    // `
    {0x0C,0x0C,0x18,0,0,0,0,0},
    // a
    {0,0,0x1E,0x30,0x3E,0x33,0x6E,0},
    // b
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0},
    // c
    {0,0,0x1E,0x33,0x03,0x33,0x1E,0},
    // d
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0},
    // e
    {0,0,0x1E,0x33,0x3F,0x03,0x1E,0},
    // f
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0},
    // g
    {0,0,0x6E,0x33,0x33,0x3E,0x30,0x1F},
    // h
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0},
    // i
    {0x0C,0,0x0E,0x0C,0x0C,0x0C,0x1E,0},
    // j
    {0x30,0,0x30,0x30,0x30,0x33,0x33,0x1E},
    // k
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0},
    // l
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
    // m
    {0,0,0x33,0x7F,0x7F,0x6B,0x63,0},
    // n
    {0,0,0x1F,0x33,0x33,0x33,0x33,0},
    // o
    {0,0,0x1E,0x33,0x33,0x33,0x1E,0},
    // p
    {0,0,0x3B,0x66,0x66,0x3E,0x06,0x0F},
    // q
    {0,0,0x6E,0x33,0x33,0x3E,0x30,0x78},
    // r
    {0,0,0x3B,0x6E,0x66,0x06,0x0F,0},
    // s
    {0,0,0x3E,0x03,0x1E,0x30,0x1F,0},
    // t
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0},
    // u
    {0,0,0x33,0x33,0x33,0x33,0x6E,0},
    // v
    {0,0,0x33,0x33,0x33,0x1E,0x0C,0},
    // w
    {0,0,0x63,0x6B,0x7F,0x7F,0x36,0},
    // x
    {0,0,0x63,0x36,0x1C,0x36,0x63,0},
    // y
    {0,0,0x33,0x33,0x33,0x3E,0x30,0x1F},
    // z
    {0,0,0x3F,0x19,0x0C,0x26,0x3F,0},
    // {
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0},
    // |
    {0x18,0x18,0x18,0,0x18,0x18,0x18,0},
    // }
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0},
    // ~
    {0x6E,0x3B,0,0,0,0,0,0},
    // 0x7F (placeholder)
    {0,0,0,0,0,0,0,0},
};

constexpr u32 kFontGlyph = 8;
constexpr u32 kFontCols  = 16;
constexpr u32 kFontRows  = 6;
constexpr u32 kFontW = kFontCols * kFontGlyph;  // 128
constexpr u32 kFontH = kFontRows * kFontGlyph;  // 48

static void build_font_atlas(std::vector<u8>& rgba) {
    rgba.assign(kFontW * kFontH * 4, 0);
    for (u32 i = 0; i < 96; ++i) {
        u32 col = i % kFontCols;
        u32 row = i / kFontCols;
        for (u32 y = 0; y < kFontGlyph; ++y) {
            u8 bits = kFont8x8[i][y];
            for (u32 x = 0; x < kFontGlyph; ++x) {
                bool on = (bits >> x) & 1u;
                u32 px = col * kFontGlyph + x;
                u32 py = row * kFontGlyph + y;
                u32 idx = (py * kFontW + px) * 4;
                u8 v = on ? 0xFF : 0x00;
                rgba[idx + 0] = 0xFF;
                rgba[idx + 1] = 0xFF;
                rgba[idx + 2] = 0xFF;
                rgba[idx + 3] = v;
            }
        }
    }
}

struct CBUI { math::vec4 screen; };

Context::~Context() { shutdown(); }

bool Context::init(gfx::Device& dev) {
    dev_ = &dev;
    auto& sc = gfx::ShaderCompiler::get();
    auto vs = sc.compile_file("UI.hlsl", "UI_VS", gfx::ShaderStage::Vertex);
    auto ps = sc.compile_file("UI.hlsl", "UI_PS", gfx::ShaderStage::Pixel);
    if (!vs.ok || !ps.ok) return false;
    dev.d3d()->CreateVertexShader(vs.bytecode.data(), vs.bytecode.size(), nullptr, vs_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps.bytecode.data(), ps.bytecode.size(),  nullptr, ps_.GetAddressOf());
    D3D11_INPUT_ELEMENT_DESC ie[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 8,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    dev.d3d()->CreateInputLayout(ie, 3, vs.bytecode.data(), vs.bytecode.size(), layout_.GetAddressOf());

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.ScissorEnable = FALSE;
    dev.d3d()->CreateRasterizerState(&rd, rs_.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = FALSE;
    dev.d3d()->CreateDepthStencilState(&dsd, ds_.GetAddressOf());

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable    = TRUE;
    bd.RenderTarget[0].SrcBlend       = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend      = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev.d3d()->CreateBlendState(&bd, bs_.GetAddressOf());

    cb_.create(dev, gfx::BufferType::Constant, sizeof(CBUI));

    // Font atlas.
    std::vector<u8> rgba;
    build_font_atlas(rgba);
    gfx::Texture2DDesc fd;
    fd.width = kFontW; fd.height = kFontH;
    fd.format = gfx::TextureFormat::RGBA8;
    font_atlas_.create(dev, fd, rgba.data(), kFontW * 4);

    // 1x1 white.
    gfx::Texture2DDesc wd;
    wd.width = 1; wd.height = 1;
    wd.format = gfx::TextureFormat::RGBA8;
    u8 white_px[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    white_.create(dev, wd, white_px, 4);
    return true;
}

void Context::shutdown() {
    vb_.destroy(); ib_.destroy();
    cb_.destroy();
    font_atlas_.destroy();
    white_.destroy();
    vs_.Reset(); ps_.Reset(); layout_.Reset();
    rs_.Reset(); ds_.Reset(); bs_.Reset();
    verts_.clear(); indices_.clear(); batches_.clear();
    dev_ = nullptr;
}

void Context::begin_frame(u32 w, u32 h, math::vec2 mp, bool down) {
    screen_w_ = w; screen_h_ = h;
    mouse_was_down_ = mouse_down_;
    mouse_px_ = mp;
    mouse_down_ = down;
    verts_.clear(); indices_.clear(); batches_.clear();
    id_counter_ = 0;
    hot_id_ = 0;
    in_panel_ = false;
}

void Context::end_frame(gfx::Device& dev, ID3D11RenderTargetView* rtv) {
    if (!mouse_down_) active_id_ = 0;
    if (verts_.empty()) return;

    if (verts_.size() * sizeof(Vertex) > vb_cap_) {
        vb_.destroy();
        vb_cap_ = verts_.size() * 2 * sizeof(Vertex);
        vb_.create(dev, gfx::BufferType::Vertex, vb_cap_, nullptr, sizeof(Vertex), true);
    }
    if (indices_.size() * sizeof(u32) > ib_cap_) {
        ib_.destroy();
        ib_cap_ = indices_.size() * 2 * sizeof(u32);
        ib_.create(dev, gfx::BufferType::Index, ib_cap_, nullptr, sizeof(u32), true);
    }
    vb_.update(dev, verts_.data(),    verts_.size() * sizeof(Vertex));
    ib_.update(dev, indices_.data(),  indices_.size() * sizeof(u32));

    auto* ctx = dev.context();
    CBUI ub{};
    ub.screen = math::vec4((f32)screen_w_, (f32)screen_h_, 0, 0);
    cb_.update(dev, &ub, sizeof(ub));

    ID3D11RenderTargetView* rtvs[1] = { rtv };
    ctx->OMSetRenderTargets(1, rtvs, nullptr);
    D3D11_VIEWPORT vp{};
    vp.Width = (FLOAT)screen_w_;
    vp.Height = (FLOAT)screen_h_;
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(layout_.Get());
    ID3D11Buffer* vb = vb_.d3d();
    UINT stride = sizeof(Vertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(ib_.d3d(), DXGI_FORMAT_R32_UINT, 0);
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);
    ID3D11Buffer* cbp = cb_.d3d();
    ctx->VSSetConstantBuffers(0, 1, &cbp);
    ID3D11SamplerState* smp = gfx::Sampler::linear_clamp();
    ctx->PSSetSamplers(0, 1, &smp);
    ctx->RSSetState(rs_.Get());
    ctx->OMSetDepthStencilState(ds_.Get(), 0);
    f32 bf[4] = { 1, 1, 1, 1 };
    ctx->OMSetBlendState(bs_.Get(), bf, 0xFFFFFFFF);

    u32 first = 0;
    for (auto& b : batches_) {
        ID3D11ShaderResourceView* srv = b.second;
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->DrawIndexed((UINT)b.first, first, 0);
        first += (UINT)b.first;
    }

    ID3D11ShaderResourceView* nul = nullptr;
    ctx->PSSetShaderResources(0, 1, &nul);
}

void Context::push_quad_(math::vec2 p, math::vec2 sz, math::vec2 uv0, math::vec2 uv1, math::vec4 c,
                         ID3D11ShaderResourceView* tex)
{
    u32 base = static_cast<u32>(verts_.size());
    verts_.push_back({ p,                 uv0,                          c });
    verts_.push_back({ p + math::vec2(sz.x, 0), { uv1.x, uv0.y },       c });
    verts_.push_back({ p + sz,            uv1,                          c });
    verts_.push_back({ p + math::vec2(0, sz.y), { uv0.x, uv1.y },       c });
    indices_.push_back(base + 0); indices_.push_back(base + 1); indices_.push_back(base + 2);
    indices_.push_back(base + 0); indices_.push_back(base + 2); indices_.push_back(base + 3);
    if (batches_.empty() || batches_.back().second != tex) {
        batches_.push_back({ 6, tex });
    } else {
        batches_.back().first += 6;
    }
}

void Context::rect(math::vec2 p, math::vec2 sz, math::vec4 c) {
    push_quad_(p, sz, {0, 0}, {1, 1}, c, white_.srv());
}

void Context::text(math::vec2 p, const std::string& s, math::vec4 color, f32 scale) {
    f32 gw = 8.0f * scale;
    f32 gh = 8.0f * scale;
    math::vec2 cur = p;
    for (char c : s) {
        if (c == '\n') {
            cur.x = p.x;
            cur.y += gh + 1;
            continue;
        }
        if ((u8)c < 0x20 || (u8)c > 0x7F) c = '?';
        u32 idx = (u8)c - 0x20;
        u32 col = idx % kFontCols;
        u32 row = idx / kFontCols;
        f32 u0 = (f32)(col * kFontGlyph) / kFontW;
        f32 v0 = (f32)(row * kFontGlyph) / kFontH;
        f32 u1 = (f32)((col + 1) * kFontGlyph) / kFontW;
        f32 v1 = (f32)((row + 1) * kFontGlyph) / kFontH;
        push_quad_(cur, {gw, gh}, {u0, v0}, {u1, v1}, color, font_atlas_.srv());
        cur.x += gw;
    }
}

void Context::image(math::vec2 p, math::vec2 sz, ID3D11ShaderResourceView* srv, math::vec4 tint) {
    push_quad_(p, sz, {0, 0}, {1, 1}, tint, srv);
}

bool Context::button(math::vec2 p, math::vec2 sz, const std::string& label) {
    u32 id = ++id_counter_;
    bool over = mouse_px_.x >= p.x && mouse_px_.x <= p.x + sz.x &&
                mouse_px_.y >= p.y && mouse_px_.y <= p.y + sz.y;
    if (over) hot_id_ = id;
    if (over && mouse_down_ && !mouse_was_down_) active_id_ = id;
    bool clicked = active_id_ == id && !mouse_down_ && over;
    math::vec4 c = style_.button_bg;
    if (active_id_ == id) c = style_.button_active;
    else if (over)        c = style_.button_hot;
    rect(p, sz, c);
    auto sz_text = measure_text(label);
    math::vec2 tp = p + (sz - sz_text) * 0.5f;
    text(tp, label, style_.text);
    return clicked;
}

void Context::label(math::vec2 p, const std::string& s, math::vec4 c) {
    text(p, s, c);
}

void Context::progress_bar(math::vec2 p, math::vec2 sz, f32 t, math::vec4 fg) {
    rect(p, sz, math::vec4(0.05f, 0.06f, 0.08f, 0.9f));
    f32 ct = math::saturate(t);
    rect(p + math::vec2(2, 2), { (sz.x - 4) * ct, sz.y - 4 }, fg);
}

void Context::begin_panel(math::vec2 origin, math::vec2 size, const std::string& title) {
    panel_origin_ = origin;
    panel_size_   = size;
    in_panel_     = true;
    cursor_       = origin + math::vec2(style_.padding, style_.padding);
    rect(origin, size, style_.panel_bg);
    if (!title.empty()) {
        rect(origin, { size.x, 20 }, style_.button_bg);
        text(origin + math::vec2(8, 6), title, style_.text);
        cursor_.y += 22;
    }
}

void Context::end_panel() { in_panel_ = false; }

bool Context::layout_button(const std::string& label) {
    if (!in_panel_) return false;
    math::vec2 sz(panel_size_.x - style_.padding * 2.0f, 22.0f);
    bool clicked = button(cursor_, sz, label);
    cursor_.y += sz.y + 4;
    return clicked;
}
void Context::layout_label(const std::string& s) {
    if (!in_panel_) return;
    text(cursor_, s, style_.text);
    cursor_.y += 12;
}
void Context::layout_separator() {
    if (!in_panel_) return;
    rect(cursor_, { panel_size_.x - style_.padding * 2.0f, 1.0f }, math::vec4(1, 1, 1, 0.15f));
    cursor_.y += 6;
}

math::vec2 Context::measure_text(const std::string& s, f32 scale) const {
    return math::vec2(static_cast<f32>(s.size()) * 8.0f * scale, 8.0f * scale);
}

Context& global() {
    static Context c;
    return c;
}

} // namespace cn::ui
