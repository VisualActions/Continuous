// Continuous Engine - high-level Renderer.
//
// Frame structure:
//   1. Cull visible draws against camera frustum.
//   2. Shadow pass: render directional CSM (4 cascades) into a shadow array
//      atlas. Lights with shadow_cast=true emit cascade VPs computed from the
//      camera frustum.
//   3. Geometry pass: render to HDR target with PBR forward shader. Reads
//      shadow atlas + IBL (irradiance + specular prefilter + BRDF LUT).
//   4. Skybox.
//   5. Post: bloom (downsample + upsample), tonemap (ACES), FXAA.
//   6. Debug draw lines on top.
//
// All the per-frame state (camera, lights, queued draws) is pushed into the
// renderer with submit_*() before frame() is called.
#pragma once

#include "continuous/gfx/Device.h"
#include "continuous/gfx/Resources.h"
#include "continuous/gfx/SwapChain.h"
#include "continuous/gfx/Mesh.h"
#include "continuous/gfx/Material.h"
#include "continuous/gfx/ShadowAtlas.h"
#include "continuous/gfx/PostProcess.h"
#include "continuous/gfx/IBL.h"
#include "continuous/gfx/DebugDraw.h"
#include "continuous/math/Math.h"

#include <vector>

namespace cn::gfx {

struct CN_API CameraData {
    math::vec3 position{0, 0, 0};
    math::mat4 view = math::mat4(1.0f);
    math::mat4 projection = math::mat4(1.0f);
    f32        near_z = 0.1f;
    f32        far_z  = 500.0f;
    f32        fov_y_rad = math::rad(60.0f);
    math::vec4 clear_color{0.05f, 0.06f, 0.08f, 1.0f};
};

enum class LightType : u32 { Directional = 0, Point = 1, Spot = 2 };

struct CN_API LightData {
    LightType  type = LightType::Directional;
    math::vec3 position {0, 5, 0};
    math::vec3 direction{0,-1, 0.2f};
    math::vec3 color    {1, 1, 1};
    f32        intensity = 1.0f;
    f32        range     = 25.0f;
    f32        spot_inner = math::rad(20.0f);
    f32        spot_outer = math::rad(30.0f);
    bool       casts_shadow = false;
};

struct CN_API DrawItem {
    Mesh*         mesh = nullptr;
    u32           submesh = 0;
    Material*     material = nullptr;
    math::mat4    transform = math::mat4(1.0f);
    math::AABB    world_aabb;
};

class CN_API Renderer {
public:
    Renderer() = default;
    ~Renderer();
    CN_NONCOPYABLE(Renderer);

    bool init(Device& dev, SwapChain& sc);
    void shutdown();

    void on_resize(u32 w, u32 h);

    // Per-frame submission.
    void begin_frame();
    void set_camera(const CameraData& cam) { camera_ = cam; }
    void submit_light(const LightData& l)  { lights_.push_back(l); }
    void submit_draw(const DrawItem& d)    { items_.push_back(d); }
    void set_ibl(IBL* ibl) { ibl_ = ibl; }
    void set_skybox(Texture* sky) { skybox_ = sky; }
    void render_to_swapchain();
    void end_frame();

    // For editor: render the scene into an offscreen RT, return its SRV.
    ID3D11ShaderResourceView* offscreen_srv() const;
    void render_offscreen(u32 w, u32 h);

    DebugDraw& debug() { return debug_; }
    PostProcess& post() { return post_; }
    ShadowAtlas& shadows() { return shadows_; }

    u32 stat_draw_calls() const { return stat_draws_; }
    u32 stat_visible_items() const { return stat_visible_; }

private:
    bool create_pipelines_();
    void create_offscreen_(u32 w, u32 h);
    void cull_();
    void shadow_pass_();
    void geometry_pass_(ID3D11RenderTargetView* rtv, u32 w, u32 h);
    void skybox_pass_(ID3D11RenderTargetView* rtv);
    void update_camera_constants_();

    Device*    dev_ = nullptr;
    SwapChain* swap_ = nullptr;

    // HDR offscreen target.
    Texture2D       hdr_color_;
    Texture2D       hdr_depth_;
    Texture2D       ldr_resolved_;
    u32             rt_w_ = 0;
    u32             rt_h_ = 0;

    // Pipeline.
    Com<ID3D11InputLayout>  input_layout_;
    Com<ID3D11VertexShader> vs_pbr_;
    Com<ID3D11PixelShader>  ps_pbr_;
    Com<ID3D11VertexShader> vs_shadow_;
    Com<ID3D11PixelShader>  ps_shadow_;
    Com<ID3D11VertexShader> vs_skybox_;
    Com<ID3D11PixelShader>  ps_skybox_;
    Com<ID3D11RasterizerState> rs_default_;
    Com<ID3D11RasterizerState> rs_double_;
    Com<ID3D11RasterizerState> rs_shadow_;
    Com<ID3D11DepthStencilState> ds_default_;
    Com<ID3D11DepthStencilState> ds_skybox_;
    Com<ID3D11BlendState>      bs_opaque_;
    Com<ID3D11BlendState>      bs_alpha_;

    // Constant buffers.
    Buffer cb_frame_;
    Buffer cb_object_;
    Buffer cb_material_;
    Buffer cb_shadow_;

    // Per-frame data.
    CameraData             camera_;
    std::vector<LightData> lights_;
    std::vector<DrawItem>  items_;
    std::vector<u32>       visible_;
    Texture*               skybox_ = nullptr;
    IBL*                   ibl_ = nullptr;

    // Subsystems.
    ShadowAtlas shadows_;
    PostProcess post_;
    DebugDraw   debug_;

    // Stats.
    u32 stat_draws_   = 0;
    u32 stat_visible_ = 0;
};

} // namespace cn::gfx
