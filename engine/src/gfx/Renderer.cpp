#include "continuous/gfx/Renderer.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Assert.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#include <algorithm>
#include <cstring>

namespace cn::gfx {

// ------- Constant buffer GPU layouts ---------------------------------------

struct LightGPU {
    math::vec3 position; f32 range;
    math::vec3 direction; f32 intensity;
    math::vec3 color;     u32 type;
    f32 spot_inner_cos; f32 spot_outer_cos;
    u32 casts_shadow;   f32 _pad;
};

struct CascadeGPU {
    math::mat4 view_proj;
    f32 split_depth;
    math::vec3 _pad;
};

struct CBFrame {
    math::mat4 view;
    math::mat4 proj;
    math::mat4 view_proj;
    math::vec4 camera_pos;
    math::vec4 screen_size;
    u32 light_count;
    u32 use_ibl;
    u32 use_csm;
    u32 csm_count;
    LightGPU   lights[16];
    CascadeGPU cascades[kShadowCascades];
};

struct CBObject {
    math::mat4 model;
    math::mat4 model_inv_t;
    math::vec4 object_color;
};

struct CBMaterial {
    math::vec4 base_color;
    math::vec4 emissive;
    f32 metallic;
    f32 roughness;
    f32 ao;
    f32 normal_strength;
    u32 has_basecolor_tex;
    u32 has_normal_tex;
    u32 has_mr_tex;
    u32 has_emissive_tex;
};

struct CBShadow {
    math::mat4 view_proj;
    u32 cascade_index;
    math::vec3 _pad;
};

Renderer::~Renderer() { shutdown(); }

bool Renderer::init(Device& dev, SwapChain& sc) {
    dev_  = &dev;
    swap_ = &sc;

    Sampler::initialize(dev);
    init_defaults(dev);

    if (!shadows_.init(dev, 1024)) return false;
    if (!post_.init(dev))           return false;
    if (!debug_.init(dev))          return false;
    if (!create_pipelines_())       return false;

    if (!cb_frame_.create(dev,    BufferType::Constant, sizeof(CBFrame)))    return false;
    if (!cb_object_.create(dev,   BufferType::Constant, sizeof(CBObject)))   return false;
    if (!cb_material_.create(dev, BufferType::Constant, sizeof(CBMaterial))) return false;
    if (!cb_shadow_.create(dev,   BufferType::Constant, sizeof(CBShadow)))   return false;

    create_offscreen_(sc.width(), sc.height());
    post_.resize(dev, sc.width(), sc.height());
    return true;
}

bool Renderer::create_pipelines_() {
    auto& sc = ShaderCompiler::get();
    auto vs_pbr = sc.compile_file("PBR.hlsl",    "StandardVS", ShaderStage::Vertex);
    auto ps_pbr = sc.compile_file("PBR.hlsl",    "StandardPS", ShaderStage::Pixel);
    auto vs_sh  = sc.compile_file("Shadow.hlsl", "ShadowVS",   ShaderStage::Vertex);
    auto ps_sh  = sc.compile_file("Shadow.hlsl", "ShadowPS",   ShaderStage::Pixel);
    auto vs_sk  = sc.compile_file("Skybox.hlsl", "SkyboxVS",   ShaderStage::Vertex);
    auto ps_sk  = sc.compile_file("Skybox.hlsl", "SkyboxPS",   ShaderStage::Pixel);
    if (!vs_pbr.ok || !ps_pbr.ok || !vs_sh.ok || !ps_sh.ok || !vs_sk.ok || !ps_sk.ok) return false;

    auto& d = *dev_;
    d.d3d()->CreateVertexShader(vs_pbr.bytecode.data(), vs_pbr.bytecode.size(), nullptr, vs_pbr_.GetAddressOf());
    d.d3d()->CreatePixelShader (ps_pbr.bytecode.data(), ps_pbr.bytecode.size(), nullptr, ps_pbr_.GetAddressOf());
    d.d3d()->CreateVertexShader(vs_sh.bytecode.data(), vs_sh.bytecode.size(),   nullptr, vs_shadow_.GetAddressOf());
    d.d3d()->CreatePixelShader (ps_sh.bytecode.data(), ps_sh.bytecode.size(),   nullptr, ps_shadow_.GetAddressOf());
    d.d3d()->CreateVertexShader(vs_sk.bytecode.data(), vs_sk.bytecode.size(),   nullptr, vs_skybox_.GetAddressOf());
    d.d3d()->CreatePixelShader (ps_sk.bytecode.data(), ps_sk.bytecode.size(),   nullptr, ps_skybox_.GetAddressOf());

    D3D11_INPUT_ELEMENT_DESC ie[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    d.d3d()->CreateInputLayout(ie, _countof(ie), vs_pbr.bytecode.data(), vs_pbr.bytecode.size(),
                               input_layout_.GetAddressOf());

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_BACK;
    rd.FrontCounterClockwise = FALSE;
    rd.DepthClipEnable = TRUE;
    rd.MultisampleEnable = FALSE;
    d.d3d()->CreateRasterizerState(&rd, rs_default_.GetAddressOf());

    rd.CullMode = D3D11_CULL_NONE;
    d.d3d()->CreateRasterizerState(&rd, rs_double_.GetAddressOf());

    rd.CullMode = D3D11_CULL_BACK;
    rd.DepthBias = 1500;
    rd.SlopeScaledDepthBias = 1.5f;
    rd.DepthBiasClamp = 0.0f;
    d.d3d()->CreateRasterizerState(&rd, rs_shadow_.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable    = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    dsd.StencilEnable  = FALSE;
    d.d3d()->CreateDepthStencilState(&dsd, ds_default_.GetAddressOf());

    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = D3D11_COMPARISON_LESS_EQUAL;
    d.d3d()->CreateDepthStencilState(&dsd, ds_skybox_.GetAddressOf());

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable = FALSE;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    d.d3d()->CreateBlendState(&bd, bs_opaque_.GetAddressOf());

    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bd.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    d.d3d()->CreateBlendState(&bd, bs_alpha_.GetAddressOf());
    return true;
}

void Renderer::create_offscreen_(u32 w, u32 h) {
    rt_w_ = std::max(1u, w);
    rt_h_ = std::max(1u, h);

    Texture2DDesc cd;
    cd.width = rt_w_; cd.height = rt_h_;
    cd.format = TextureFormat::RGBA16F;
    cd.render_target = true;
    hdr_color_.destroy();
    hdr_color_.create(*dev_, cd);

    Texture2DDesc dd;
    dd.width = rt_w_; dd.height = rt_h_;
    dd.format = TextureFormat::D32F;
    dd.render_target = true;
    hdr_depth_.destroy();
    hdr_depth_.create(*dev_, dd);

    Texture2DDesc lr;
    lr.width = rt_w_; lr.height = rt_h_;
    lr.format = TextureFormat::SRGBA8;
    lr.render_target = true;
    ldr_resolved_.destroy();
    ldr_resolved_.create(*dev_, lr);
}

void Renderer::shutdown() {
    if (!dev_) return;
    cb_frame_.destroy();
    cb_object_.destroy();
    cb_material_.destroy();
    cb_shadow_.destroy();
    debug_.destroy();
    post_.destroy();
    shadows_.destroy();
    Sampler::shutdown();
    shutdown_defaults();
    hdr_color_.destroy();
    hdr_depth_.destroy();
    ldr_resolved_.destroy();
    input_layout_.Reset();
    vs_pbr_.Reset(); ps_pbr_.Reset();
    vs_shadow_.Reset(); ps_shadow_.Reset();
    vs_skybox_.Reset(); ps_skybox_.Reset();
    rs_default_.Reset(); rs_double_.Reset(); rs_shadow_.Reset();
    ds_default_.Reset(); ds_skybox_.Reset();
    bs_opaque_.Reset(); bs_alpha_.Reset();
    dev_ = nullptr;
}

void Renderer::on_resize(u32 w, u32 h) {
    if (!dev_) return;
    create_offscreen_(w, h);
    post_.resize(*dev_, w, h);
}

void Renderer::begin_frame() {
    items_.clear();
    lights_.clear();
    visible_.clear();
    stat_draws_   = 0;
    stat_visible_ = 0;
}

void Renderer::end_frame() {
    // Clean up after use.
}

void Renderer::cull_() {
    math::Frustum f = math::Frustum::from_view_projection(camera_.projection * camera_.view);
    visible_.clear();
    visible_.reserve(items_.size());
    for (u32 i = 0; i < items_.size(); ++i) {
        if (f.intersects(items_[i].world_aabb)) visible_.push_back(i);
    }
    stat_visible_ = static_cast<u32>(visible_.size());
}

void Renderer::shadow_pass_() {
    if (!shadows_.atlas_srv()) return;
    // Find directional light.
    const LightData* dir = nullptr;
    for (auto& l : lights_) if (l.type == LightType::Directional && l.casts_shadow) { dir = &l; break; }
    if (!dir) return;

    shadows_.compute_cascades(camera_.view, camera_.projection, camera_.near_z, camera_.far_z, dir->direction);

    auto* ctx = dev_->context();
    ctx->RSSetState(rs_shadow_.Get());
    ctx->OMSetDepthStencilState(ds_default_.Get(), 0);
    ctx->IASetInputLayout(input_layout_.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_shadow_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_shadow_.Get(), nullptr, 0);
    ID3D11Buffer* cbs[2] = { cb_object_.d3d(), cb_shadow_.d3d() };
    (void)cbs;

    D3D11_VIEWPORT vp{};
    vp.Width = vp.Height = (FLOAT)shadows_.cascade_size();
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    for (u32 cas = 0; cas < kShadowCascades; ++cas) {
        ID3D11DepthStencilView* dsv = shadows_.dsv(cas);
        if (!dsv) continue;
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
        ID3D11RenderTargetView* none = nullptr;
        ctx->OMSetRenderTargets(0, &none, dsv);

        CBShadow shc{};
        shc.view_proj = shadows_.cascade(cas).view_proj;
        shc.cascade_index = cas;
        cb_shadow_.update(*dev_, &shc, sizeof(shc));
        ID3D11Buffer* cb = cb_shadow_.d3d();
        ctx->VSSetConstantBuffers(3, 1, &cb);

        for (u32 idx : visible_) {
            const DrawItem& it = items_[idx];
            if (!it.mesh) continue;
            CBObject ob{};
            ob.model = it.transform;
            ob.model_inv_t = glm::transpose(glm::inverse(it.transform));
            cb_object_.update(*dev_, &ob, sizeof(ob));
            ID3D11Buffer* cbo = cb_object_.d3d();
            ctx->VSSetConstantBuffers(1, 1, &cbo);

            ID3D11Buffer* vb = it.mesh->vbo().d3d();
            ID3D11Buffer* ib = it.mesh->ibo().d3d();
            UINT stride = sizeof(Vertex), offset = 0;
            ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
            ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);

            const auto& sm = it.mesh->subs()[it.submesh < it.mesh->subs().size() ? it.submesh : 0];
            ctx->DrawIndexed(sm.index_count, sm.first_index, 0);
            ++stat_draws_;
        }
    }
}

void Renderer::update_camera_constants_() {
    CBFrame f{};
    f.view = camera_.view;
    f.proj = camera_.projection;
    f.view_proj = camera_.projection * camera_.view;
    f.camera_pos = math::vec4(camera_.position, 0.0f);
    f.screen_size = math::vec4((f32)rt_w_, (f32)rt_h_, 1.0f / (f32)rt_w_, 1.0f / (f32)rt_h_);
    f.light_count = static_cast<u32>(std::min<usize>(lights_.size(), 16));
    f.use_ibl     = ibl_ ? 1 : 0;
    f.csm_count   = kShadowCascades;
    bool any_dir_shadow = false;
    for (u32 i = 0; i < f.light_count; ++i) {
        const auto& src = lights_[i];
        LightGPU& dst = f.lights[i];
        dst.position = src.position;
        dst.direction = src.direction;
        dst.color = src.color;
        dst.intensity = src.intensity;
        dst.range = src.range;
        dst.spot_inner_cos = std::cos(src.spot_inner);
        dst.spot_outer_cos = std::cos(src.spot_outer);
        dst.type = static_cast<u32>(src.type);
        dst.casts_shadow = src.casts_shadow ? 1u : 0u;
        if (src.type == LightType::Directional && src.casts_shadow) any_dir_shadow = true;
    }
    f.use_csm = any_dir_shadow ? 1 : 0;
    for (u32 c = 0; c < kShadowCascades; ++c) {
        f.cascades[c].view_proj = shadows_.cascade(c).view_proj;
        f.cascades[c].split_depth = shadows_.cascade(c).split_depth;
    }
    cb_frame_.update(*dev_, &f, sizeof(f));
}

void Renderer::geometry_pass_(ID3D11RenderTargetView* rtv, u32 w, u32 h) {
    auto* ctx = dev_->context();
    ID3D11RenderTargetView* rtvs[1] = { rtv };
    ID3D11DepthStencilView* dsv = hdr_depth_.dsv();
    f32 clear[4] = {
        camera_.clear_color.r, camera_.clear_color.g,
        camera_.clear_color.b, camera_.clear_color.a
    };
    ctx->ClearRenderTargetView(rtv, clear);
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ctx->OMSetRenderTargets(1, rtvs, dsv);

    D3D11_VIEWPORT vp{}; vp.Width = (FLOAT)w; vp.Height = (FLOAT)h; vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->IASetInputLayout(input_layout_.Get());
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_pbr_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_pbr_.Get(), nullptr, 0);
    ctx->OMSetDepthStencilState(ds_default_.Get(), 0);
    f32 bf[4] = {1,1,1,1};
    ctx->OMSetBlendState(bs_opaque_.Get(), bf, 0xFFFFFFFF);
    ctx->RSSetState(rs_default_.Get());

    ID3D11Buffer* cb_f = cb_frame_.d3d();
    ID3D11Buffer* cb_o = cb_object_.d3d();
    ID3D11Buffer* cb_m = cb_material_.d3d();
    ctx->VSSetConstantBuffers(0, 1, &cb_f);
    ctx->PSSetConstantBuffers(0, 1, &cb_f);
    ctx->VSSetConstantBuffers(1, 1, &cb_o);
    ctx->PSSetConstantBuffers(1, 1, &cb_o);
    ctx->VSSetConstantBuffers(2, 1, &cb_m);
    ctx->PSSetConstantBuffers(2, 1, &cb_m);

    ID3D11SamplerState* samps[3] = {
        Sampler::anisotropic_wrap(),
        Sampler::linear_clamp(),
        Sampler::shadow_compare()
    };
    ctx->PSSetSamplers(0, 3, samps);

    ID3D11ShaderResourceView* irr  = ibl_ ? ibl_->irradiance_srv() : nullptr;
    ID3D11ShaderResourceView* pref = ibl_ ? ibl_->prefilter_srv()  : nullptr;
    ID3D11ShaderResourceView* brdf = ibl_ ? ibl_->brdf_lut_srv()   : nullptr;
    ID3D11ShaderResourceView* shdw = shadows_.atlas_srv();
    ID3D11ShaderResourceView* sky_srv = skybox_ ? skybox_->srv() : (ibl_ ? ibl_->environment_srv() : nullptr);

    for (u32 idx : visible_) {
        const DrawItem& it = items_[idx];
        if (!it.mesh || !it.material) continue;

        // Fill object cb.
        CBObject ob{};
        ob.model = it.transform;
        ob.model_inv_t = glm::transpose(glm::inverse(it.transform));
        ob.object_color = it.material->base_color;
        cb_object_.update(*dev_, &ob, sizeof(ob));

        // Fill material cb.
        CBMaterial mc{};
        mc.base_color = it.material->base_color;
        mc.emissive   = math::vec4(it.material->emissive, 0);
        mc.metallic = it.material->metallic;
        mc.roughness = it.material->roughness;
        mc.ao = it.material->ao;
        mc.normal_strength = it.material->normal_strength;
        mc.has_basecolor_tex = it.material->base_color_tex ? 1u : 0u;
        mc.has_normal_tex    = it.material->normal_tex ? 1u : 0u;
        mc.has_mr_tex        = it.material->metallic_roughness_tex ? 1u : 0u;
        mc.has_emissive_tex  = it.material->emissive_tex ? 1u : 0u;
        cb_material_.update(*dev_, &mc, sizeof(mc));

        // Bind textures.
        ID3D11ShaderResourceView* srvs[10] = {
            it.material->base_color_tex          ? it.material->base_color_tex->srv()          : white_pixel()->srv(),
            it.material->normal_tex              ? it.material->normal_tex->srv()              : normal_default()->srv(),
            it.material->metallic_roughness_tex  ? it.material->metallic_roughness_tex->srv()  : white_pixel()->srv(),
            it.material->emissive_tex            ? it.material->emissive_tex->srv()            : black_pixel()->srv(),
            it.material->ao_tex                  ? it.material->ao_tex->srv()                  : white_pixel()->srv(),
            irr, pref, brdf, shdw,
            sky_srv
        };
        ctx->PSSetShaderResources(0, 10, srvs);

        // Rasterizer for double-sided / blend for transparent.
        ctx->RSSetState(it.material->double_sided ? rs_double_.Get() : rs_default_.Get());
        ctx->OMSetBlendState(it.material->transparent ? bs_alpha_.Get() : bs_opaque_.Get(), bf, 0xFFFFFFFF);

        // Draw.
        ID3D11Buffer* vb = it.mesh->vbo().d3d();
        ID3D11Buffer* ib = it.mesh->ibo().d3d();
        UINT stride = sizeof(Vertex), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R32_UINT, 0);
        const auto& sm = it.mesh->subs()[it.submesh < it.mesh->subs().size() ? it.submesh : 0];
        ctx->DrawIndexed(sm.index_count, sm.first_index, 0);
        ++stat_draws_;
    }

    ID3D11ShaderResourceView* nuls[10] = {};
    ctx->PSSetShaderResources(0, 10, nuls);
}

void Renderer::skybox_pass_(ID3D11RenderTargetView* rtv) {
    auto* ctx = dev_->context();
    ID3D11ShaderResourceView* sky = skybox_ ? skybox_->srv() : (ibl_ ? ibl_->environment_srv() : nullptr);
    if (!sky) return;
    ID3D11RenderTargetView* rtvs[1] = { rtv };
    ctx->OMSetRenderTargets(1, rtvs, hdr_depth_.dsv());
    ctx->IASetInputLayout(nullptr);
    ID3D11Buffer* none = nullptr; UINT z = 0;
    ctx->IASetVertexBuffers(0, 1, &none, &z, &z);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs_skybox_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_skybox_.Get(), nullptr, 0);
    ID3D11Buffer* cb = cb_frame_.d3d();
    ctx->VSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetConstantBuffers(0, 1, &cb);
    ID3D11SamplerState* s = Sampler::linear_clamp();
    ctx->PSSetSamplers(0, 1, &s);
    // Bind sky on slot 9 (matches PBR.hlsl t_skybox register).
    ID3D11ShaderResourceView* srvs[10] = {
        nullptr,nullptr,nullptr,nullptr,nullptr,
        nullptr,nullptr,nullptr,nullptr, sky
    };
    ctx->PSSetShaderResources(0, 10, srvs);
    ctx->OMSetDepthStencilState(ds_skybox_.Get(), 0);
    ctx->RSSetState(rs_default_.Get());
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* nuls[10] = {};
    ctx->PSSetShaderResources(0, 10, nuls);
}

void Renderer::render_to_swapchain() {
    if (!dev_ || !swap_) return;
    if (rt_w_ != swap_->width() || rt_h_ != swap_->height()) on_resize(swap_->width(), swap_->height());

    cull_();
    update_camera_constants_();
    shadow_pass_();
    geometry_pass_(hdr_color_.rtv(), rt_w_, rt_h_);
    skybox_pass_(hdr_color_.rtv());

    // Debug lines on top of HDR (in scene space).
    debug_.render(*dev_, hdr_color_.rtv(), hdr_depth_.dsv(), rt_w_, rt_h_,
                  camera_.projection * camera_.view);

    post_.run(*dev_, hdr_color_.srv(), swap_->back_buffer_rtv(), rt_w_, rt_h_);

    // Reset bindings.
    auto* ctx = dev_->context();
    ID3D11RenderTargetView* rtv = swap_->back_buffer_rtv();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
}

void Renderer::render_offscreen(u32 w, u32 h) {
    if (rt_w_ != w || rt_h_ != h) on_resize(w, h);

    cull_();
    update_camera_constants_();
    shadow_pass_();
    geometry_pass_(hdr_color_.rtv(), rt_w_, rt_h_);
    skybox_pass_(hdr_color_.rtv());
    debug_.render(*dev_, hdr_color_.rtv(), hdr_depth_.dsv(), rt_w_, rt_h_,
                  camera_.projection * camera_.view);
    post_.run(*dev_, hdr_color_.srv(), ldr_resolved_.rtv(), rt_w_, rt_h_);
}

ID3D11ShaderResourceView* Renderer::offscreen_srv() const {
    return ldr_resolved_.srv();
}

} // namespace cn::gfx
