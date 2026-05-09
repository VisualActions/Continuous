// In-game UI - immediate-mode 2D drawing for HUDs and menus.
//
// Backed by a textured quad batch + a built-in 8x8 bitmap font (embedded
// glyph data). Separate from the editor's ImGui usage so a shipped game does
// not pull in editor-only widgets.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/gfx/Resources.h"
#include "continuous/math/Math.h"

#include <string>
#include <vector>

namespace cn::ui {

struct CN_API Style {
    math::vec4 panel_bg   {0.10f, 0.11f, 0.13f, 0.85f};
    math::vec4 button_bg  {0.20f, 0.22f, 0.26f, 1.00f};
    math::vec4 button_hot {0.30f, 0.34f, 0.40f, 1.00f};
    math::vec4 button_active{0.40f, 0.46f, 0.55f, 1.00f};
    math::vec4 text       {1.00f, 1.00f, 1.00f, 1.00f};
    math::vec4 accent     {0.30f, 0.65f, 1.00f, 1.00f};
    f32        padding = 6.0f;
};

class CN_API Context {
public:
    Context() = default;
    ~Context();
    CN_NONCOPYABLE(Context);

    bool init(gfx::Device& dev);
    void shutdown();

    void begin_frame(u32 screen_w, u32 screen_h, math::vec2 mouse_px, bool mouse_down);
    void end_frame  (gfx::Device& dev, ID3D11RenderTargetView* rtv);

    Style& style() { return style_; }

    // Drawing primitives.
    void rect(math::vec2 p, math::vec2 size, math::vec4 color);
    void text(math::vec2 p, const std::string& s, math::vec4 color = {1, 1, 1, 1}, f32 scale = 1.0f);
    void image(math::vec2 p, math::vec2 size, ID3D11ShaderResourceView* srv,
               math::vec4 tint = {1, 1, 1, 1});

    // Widgets.
    bool button(math::vec2 p, math::vec2 size, const std::string& label);
    void label (math::vec2 p, const std::string& text, math::vec4 color = {1, 1, 1, 1});
    void progress_bar(math::vec2 p, math::vec2 size, f32 t01, math::vec4 fg = {0.3f, 0.7f, 1, 1});

    // Layout helpers - simple top-down panel.
    void begin_panel(math::vec2 origin, math::vec2 size, const std::string& title = "");
    void end_panel();
    bool layout_button(const std::string& label);
    void layout_label (const std::string& s);
    void layout_separator();

    math::vec2 measure_text(const std::string& s, f32 scale = 1.0f) const;

private:
    struct Vertex { math::vec2 pos; math::vec2 uv; math::vec4 color; };

    void flush_();
    bool ensure_buffers_(gfx::Device& dev, usize need);
    void push_quad_(math::vec2 p, math::vec2 sz, math::vec2 uv0, math::vec2 uv1, math::vec4 c,
                    ID3D11ShaderResourceView* tex);

    gfx::Device*  dev_ = nullptr;
    Style         style_;
    std::vector<Vertex> verts_;
    std::vector<u32>    indices_;
    std::vector<std::pair<usize, ID3D11ShaderResourceView*>> batches_; // <index_count, srv>

    gfx::Buffer        vb_, ib_;
    usize              vb_cap_ = 0, ib_cap_ = 0;
    gfx::Texture2D     font_atlas_;
    gfx::Texture2D     white_;
    Com<ID3D11VertexShader> vs_;
    Com<ID3D11PixelShader>  ps_;
    Com<ID3D11InputLayout>  layout_;
    Com<ID3D11RasterizerState> rs_;
    Com<ID3D11DepthStencilState> ds_;
    Com<ID3D11BlendState>      bs_;
    gfx::Buffer                cb_;

    // Per-frame state.
    u32           screen_w_ = 0, screen_h_ = 0;
    math::vec2    mouse_px_ {0, 0};
    bool          mouse_down_ = false;
    bool          mouse_was_down_ = false;
    u32           hot_id_ = 0, active_id_ = 0;
    u32           id_counter_ = 0;

    // Layout state.
    math::vec2    cursor_  {0, 0};
    math::vec2    panel_origin_ {0, 0};
    math::vec2    panel_size_   {0, 0};
    bool          in_panel_ = false;
};

CN_API Context& global();

} // namespace cn::ui
