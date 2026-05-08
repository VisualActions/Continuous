#include "continuous/gfx/PostProcess.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Log.h"

#include <algorithm>

namespace cn::gfx {

struct CBPost {
    math::vec4 size_inv;
    math::vec4 params;
};

bool PostProcess::init(Device& dev) {
    auto& sc = ShaderCompiler::get();
    auto vs = sc.compile_file("PostProcess.hlsl", "FullscreenVS", ShaderStage::Vertex);
    auto ps_t = sc.compile_file("PostProcess.hlsl", "ThresholdPS", ShaderStage::Pixel);
    auto ps_d = sc.compile_file("PostProcess.hlsl", "DownsamplePS", ShaderStage::Pixel);
    auto ps_u = sc.compile_file("PostProcess.hlsl", "UpsamplePS",   ShaderStage::Pixel);
    auto ps_tm= sc.compile_file("PostProcess.hlsl", "TonemapPS",    ShaderStage::Pixel);
    auto ps_x = sc.compile_file("PostProcess.hlsl", "FxaaPS",       ShaderStage::Pixel);
    if (!vs.ok || !ps_t.ok || !ps_d.ok || !ps_u.ok || !ps_tm.ok || !ps_x.ok) return false;

    dev.d3d()->CreateVertexShader(vs.bytecode.data(), vs.bytecode.size(), nullptr, vs_fullscreen_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps_t.bytecode.data(), ps_t.bytecode.size(),  nullptr, ps_threshold_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps_d.bytecode.data(), ps_d.bytecode.size(),  nullptr, ps_downsample_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps_u.bytecode.data(), ps_u.bytecode.size(),  nullptr, ps_upsample_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps_tm.bytecode.data(), ps_tm.bytecode.size(), nullptr, ps_tonemap_.GetAddressOf());
    dev.d3d()->CreatePixelShader(ps_x.bytecode.data(), ps_x.bytecode.size(),  nullptr, ps_fxaa_.GetAddressOf());

    if (!cb_post_.create(dev, BufferType::Constant, sizeof(CBPost))) return false;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    dev.d3d()->CreateSamplerState(&sd, sampler_clamp_.GetAddressOf());

    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable    = TRUE;
    bd.RenderTarget[0].SrcBlend       = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend      = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    dev.d3d()->CreateBlendState(&bd, bs_additive_.GetAddressOf());

    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev.d3d()->CreateRasterizerState(&rd, rs_.GetAddressOf());

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = FALSE;
    dsd.StencilEnable = FALSE;
    dev.d3d()->CreateDepthStencilState(&dsd, ds_.GetAddressOf());

    return true;
}

void PostProcess::destroy() {
    cb_post_.destroy();
    sampler_clamp_.Reset();
    bs_additive_.Reset();
    rs_.Reset();
    ds_.Reset();
    for (u32 m = 0; m < kBloomMips; ++m) {
        bloom_tex_[m].Reset();
        bloom_rtv_[m].Reset();
        bloom_srv_[m].Reset();
    }
    tone_tex_.Reset(); tone_rtv_.Reset(); tone_srv_.Reset();
}

void PostProcess::resize(Device& dev, u32 w, u32 h) {
    if (rt_w_ == w && rt_h_ == h) return;
    rt_w_ = w; rt_h_ = h;

    auto make_rt = [&](u32 ww, u32 hh, DXGI_FORMAT fmt,
                       Com<ID3D11Texture2D>& tex, Com<ID3D11RenderTargetView>& rtv,
                       Com<ID3D11ShaderResourceView>& srv) {
        tex.Reset(); rtv.Reset(); srv.Reset();
        D3D11_TEXTURE2D_DESC d{};
        d.Width  = std::max(1u, ww);
        d.Height = std::max(1u, hh);
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = fmt;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        dev.d3d()->CreateTexture2D(&d, nullptr, tex.GetAddressOf());
        dev.d3d()->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf());
        dev.d3d()->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
    };

    u32 mw = w / 2, mh = h / 2;
    for (u32 m = 0; m < kBloomMips; ++m) {
        bloom_w_[m] = std::max(1u, mw);
        bloom_h_[m] = std::max(1u, mh);
        make_rt(bloom_w_[m], bloom_h_[m], DXGI_FORMAT_R16G16B16A16_FLOAT,
                bloom_tex_[m], bloom_rtv_[m], bloom_srv_[m]);
        mw /= 2; mh /= 2;
    }
    make_rt(w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, tone_tex_, tone_rtv_, tone_srv_);
}

void PostProcess::run(Device& dev, ID3D11ShaderResourceView* hdr, ID3D11RenderTargetView* ldr,
                      u32 w, u32 h)
{
    auto* ctx = dev.context();

    auto set_state = [&]() {
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(nullptr);
        ID3D11Buffer* none = nullptr;
        UINT z = 0;
        ctx->IASetVertexBuffers(0, 1, &none, &z, &z);
        ctx->RSSetState(rs_.Get());
        ctx->OMSetDepthStencilState(ds_.Get(), 0);
        ctx->VSSetShader(vs_fullscreen_.Get(), nullptr, 0);
        ID3D11SamplerState* s = sampler_clamp_.Get();
        ctx->PSSetSamplers(0, 1, &s);
    };
    auto draw = [&]() { ctx->Draw(3, 0); };

    auto set_viewport = [&](u32 ww, u32 hh) {
        D3D11_VIEWPORT vp{};
        vp.Width = (FLOAT)ww;
        vp.Height = (FLOAT)hh;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
    };

    auto write_cb = [&](u32 ww, u32 hh) {
        CBPost c;
        c.size_inv = math::vec4(1.0f / std::max(1u, ww), 1.0f / std::max(1u, hh), (f32)ww, (f32)hh);
        c.params   = math::vec4(bloom_threshold, bloom_strength, exposure, gamma);
        cb_post_.update(dev, &c, sizeof(c));
        ID3D11Buffer* cb = cb_post_.d3d();
        ctx->VSSetConstantBuffers(0, 1, &cb);
        ctx->PSSetConstantBuffers(0, 1, &cb);
    };

    set_state();
    float black[4] = {0,0,0,1};

    // 1) Threshold into bloom mip 0.
    set_viewport(bloom_w_[0], bloom_h_[0]);
    ID3D11RenderTargetView* rtv0 = bloom_rtv_[0].Get();
    ctx->ClearRenderTargetView(rtv0, black);
    ctx->OMSetRenderTargets(1, &rtv0, nullptr);
    write_cb(bloom_w_[0], bloom_h_[0]);
    ID3D11ShaderResourceView* srv0 = hdr;
    ctx->PSSetShaderResources(0, 1, &srv0);
    ctx->PSSetShader(ps_threshold_.Get(), nullptr, 0);
    draw();

    // 2) Downsample chain.
    ID3D11ShaderResourceView* nullsrv = nullptr;
    for (u32 m = 1; m < kBloomMips; ++m) {
        ctx->PSSetShaderResources(0, 1, &nullsrv);
        set_viewport(bloom_w_[m], bloom_h_[m]);
        ID3D11RenderTargetView* rtvm = bloom_rtv_[m].Get();
        ctx->ClearRenderTargetView(rtvm, black);
        ctx->OMSetRenderTargets(1, &rtvm, nullptr);
        write_cb(bloom_w_[m - 1], bloom_h_[m - 1]);
        ID3D11ShaderResourceView* srvprev = bloom_srv_[m - 1].Get();
        ctx->PSSetShaderResources(0, 1, &srvprev);
        ctx->PSSetShader(ps_downsample_.Get(), nullptr, 0);
        draw();
    }

    // 3) Upsample with additive blend back up the chain.
    ID3D11BlendState* bs = bs_additive_.Get();
    float bf[4] = {1,1,1,1};
    ctx->OMSetBlendState(bs, bf, 0xFFFFFFFF);
    for (u32 m = kBloomMips - 1; m > 0; --m) {
        ctx->PSSetShaderResources(0, 1, &nullsrv);
        set_viewport(bloom_w_[m - 1], bloom_h_[m - 1]);
        ID3D11RenderTargetView* rtvm = bloom_rtv_[m - 1].Get();
        ctx->OMSetRenderTargets(1, &rtvm, nullptr);
        write_cb(bloom_w_[m], bloom_h_[m]);
        ID3D11ShaderResourceView* srvm = bloom_srv_[m].Get();
        ctx->PSSetShaderResources(0, 1, &srvm);
        ctx->PSSetShader(ps_upsample_.Get(), nullptr, 0);
        draw();
    }
    ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

    // 4) Tonemap (HDR + bloom -> LDR).
    set_viewport(w, h);
    if (fxaa_enabled) {
        ID3D11RenderTargetView* tone = tone_rtv_.Get();
        ctx->OMSetRenderTargets(1, &tone, nullptr);
    } else {
        ctx->OMSetRenderTargets(1, &ldr, nullptr);
    }
    write_cb(w, h);
    ID3D11ShaderResourceView* srvs[2] = { hdr, bloom_srv_[0].Get() };
    ctx->PSSetShaderResources(0, 2, srvs);
    ctx->PSSetShader(ps_tonemap_.Get(), nullptr, 0);
    draw();

    // 5) FXAA pass.
    if (fxaa_enabled) {
        ID3D11ShaderResourceView* clrs[2] = { nullptr, nullptr };
        ctx->PSSetShaderResources(0, 2, clrs);
        ctx->OMSetRenderTargets(1, &ldr, nullptr);
        write_cb(w, h);
        ID3D11ShaderResourceView* srv = tone_srv_.Get();
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->PSSetShader(ps_fxaa_.Get(), nullptr, 0);
        draw();
    }

    ID3D11ShaderResourceView* clrs[2] = { nullptr, nullptr };
    ctx->PSSetShaderResources(0, 2, clrs);
}

} // namespace cn::gfx
