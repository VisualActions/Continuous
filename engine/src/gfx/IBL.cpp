#include "continuous/gfx/IBL.h"
#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Log.h"
#include "continuous/math/Math.h"

namespace cn::gfx {

struct CBIBL {
    math::mat4 face_view;
    math::mat4 face_proj;
    f32        roughness;
    u32        face;
    u32        size;
    f32        pad;
};

static void face_view_proj(u32 face, math::mat4& view, math::mat4& proj) {
    // D3D cube face order: +X, -X, +Y, -Y, +Z, -Z
    math::vec3 forward, up;
    switch (face) {
        case 0: forward = { 1, 0, 0}; up = {0, 1, 0}; break;
        case 1: forward = {-1, 0, 0}; up = {0, 1, 0}; break;
        case 2: forward = { 0, 1, 0}; up = {0, 0,-1}; break;
        case 3: forward = { 0,-1, 0}; up = {0, 0, 1}; break;
        case 4: forward = { 0, 0, 1}; up = {0, 1, 0}; break;
        default:forward = { 0, 0,-1}; up = {0, 1, 0}; break;
    }
    view = math::look_at(math::vec3(0), forward, up);
    proj = math::perspective(math::kHalfPi, 1.0f, 0.1f, 100.0f);
}

bool IBL::generate_procedural(Device& dev, u32 cube_size, u32 irr_size, u32 pref_mips) {
    prefilter_mips_ = pref_mips;

    auto& sc = ShaderCompiler::get();
    auto vs   = sc.compile_file("IBL.hlsl", "FaceVS",          ShaderStage::Vertex);
    auto ps_s = sc.compile_file("IBL.hlsl", "ProceduralSkyPS", ShaderStage::Pixel);
    auto ps_i = sc.compile_file("IBL.hlsl", "IrradiancePS",    ShaderStage::Pixel);
    auto ps_p = sc.compile_file("IBL.hlsl", "PrefilterPS",     ShaderStage::Pixel);
    auto ps_b = sc.compile_file("IBL.hlsl", "BRDFLutPS",       ShaderStage::Pixel);
    if (!vs.ok || !ps_s.ok || !ps_i.ok || !ps_p.ok || !ps_b.ok) return false;

    Com<ID3D11VertexShader> vs_face;
    Com<ID3D11PixelShader>  ps_sky, ps_irr, ps_pref, ps_brdf;
    dev.d3d()->CreateVertexShader(vs.bytecode.data(),  vs.bytecode.size(),  nullptr, vs_face.GetAddressOf());
    dev.d3d()->CreatePixelShader (ps_s.bytecode.data(), ps_s.bytecode.size(), nullptr, ps_sky.GetAddressOf());
    dev.d3d()->CreatePixelShader (ps_i.bytecode.data(), ps_i.bytecode.size(), nullptr, ps_irr.GetAddressOf());
    dev.d3d()->CreatePixelShader (ps_p.bytecode.data(), ps_p.bytecode.size(), nullptr, ps_pref.GetAddressOf());
    dev.d3d()->CreatePixelShader (ps_b.bytecode.data(), ps_b.bytecode.size(), nullptr, ps_brdf.GetAddressOf());

    Buffer cb;
    if (!cb.create(dev, BufferType::Constant, sizeof(CBIBL))) return false;

    auto draw_fullscreen = [&]() { dev.context()->Draw(3, 0); };

    auto* ctx = dev.context();
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(nullptr);
    ID3D11Buffer* none = nullptr; UINT z = 0;
    ctx->IASetVertexBuffers(0, 1, &none, &z, &z);

    D3D11_RASTERIZER_DESC rd{}; rd.FillMode = D3D11_FILL_SOLID; rd.CullMode = D3D11_CULL_NONE;
    Com<ID3D11RasterizerState> rs;
    dev.d3d()->CreateRasterizerState(&rd, rs.GetAddressOf());
    ctx->RSSetState(rs.Get());

    D3D11_DEPTH_STENCIL_DESC dsd{}; dsd.DepthEnable = FALSE; dsd.StencilEnable = FALSE;
    Com<ID3D11DepthStencilState> ds;
    dev.d3d()->CreateDepthStencilState(&dsd, ds.GetAddressOf());
    ctx->OMSetDepthStencilState(ds.Get(), 0);

    D3D11_SAMPLER_DESC sd{}; sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    Com<ID3D11SamplerState> samp;
    dev.d3d()->CreateSamplerState(&sd, samp.GetAddressOf());
    ID3D11SamplerState* s = samp.Get();
    ctx->PSSetSamplers(0, 1, &s);

    auto make_cube = [&](u32 size, u32 mips,
                         Com<ID3D11Texture2D>& tex,
                         Com<ID3D11ShaderResourceView>& srv) {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = size; d.Height = size; d.MipLevels = mips; d.ArraySize = 6;
        d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (mips != 1) d.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
        dev.d3d()->CreateTexture2D(&d, nullptr, tex.GetAddressOf());
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = d.Format;
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sv.TextureCube.MipLevels = mips;
        dev.d3d()->CreateShaderResourceView(tex.Get(), &sv, srv.GetAddressOf());
    };

    auto rtv_for_face = [&](ID3D11Texture2D* t, u32 face, u32 mip,
                            Com<ID3D11RenderTargetView>& rtv) {
        D3D11_RENDER_TARGET_VIEW_DESC d{};
        d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        d.Texture2DArray.FirstArraySlice = face;
        d.Texture2DArray.ArraySize = 1;
        d.Texture2DArray.MipSlice = mip;
        dev.d3d()->CreateRenderTargetView(t, &d, rtv.GetAddressOf());
    };

    auto set_viewport = [&](u32 w, u32 h) {
        D3D11_VIEWPORT vp{}; vp.Width = (FLOAT)w; vp.Height = (FLOAT)h; vp.MaxDepth = 1; ctx->RSSetViewports(1, &vp);
    };

    // ---- 1. Procedural environment cubemap ----
    make_cube(cube_size, 1, env_tex_, env_srv_);
    ctx->VSSetShader(vs_face.Get(), nullptr, 0);
    ctx->PSSetShader(ps_sky.Get(), nullptr, 0);
    for (u32 face = 0; face < 6; ++face) {
        Com<ID3D11RenderTargetView> rtv;
        rtv_for_face(env_tex_.Get(), face, 0, rtv);
        ID3D11RenderTargetView* r = rtv.Get();
        float clear[4] = { 0, 0, 0, 1 };
        ctx->ClearRenderTargetView(r, clear);
        ctx->OMSetRenderTargets(1, &r, nullptr);
        set_viewport(cube_size, cube_size);

        CBIBL d{};
        face_view_proj(face, d.face_view, d.face_proj);
        d.face = face; d.size = cube_size; d.roughness = 0;
        cb.update(dev, &d, sizeof(d));
        ID3D11Buffer* cbp = cb.d3d();
        ctx->VSSetConstantBuffers(0, 1, &cbp);
        ctx->PSSetConstantBuffers(0, 1, &cbp);
        draw_fullscreen();
    }

    // ---- 2. Irradiance cubemap ----
    make_cube(irr_size, 1, irr_tex_, irr_srv_);
    ctx->VSSetShader(vs_face.Get(), nullptr, 0);
    ctx->PSSetShader(ps_irr.Get(), nullptr, 0);
    ID3D11ShaderResourceView* env_for_use = env_srv_.Get();
    ctx->PSSetShaderResources(0, 1, &env_for_use);
    for (u32 face = 0; face < 6; ++face) {
        Com<ID3D11RenderTargetView> rtv;
        rtv_for_face(irr_tex_.Get(), face, 0, rtv);
        ID3D11RenderTargetView* r = rtv.Get();
        ctx->OMSetRenderTargets(1, &r, nullptr);
        set_viewport(irr_size, irr_size);
        CBIBL d{};
        face_view_proj(face, d.face_view, d.face_proj);
        d.face = face; d.size = irr_size; d.roughness = 0;
        cb.update(dev, &d, sizeof(d));
        ID3D11Buffer* cbp = cb.d3d();
        ctx->VSSetConstantBuffers(0, 1, &cbp);
        ctx->PSSetConstantBuffers(0, 1, &cbp);
        draw_fullscreen();
    }
    ID3D11ShaderResourceView* nul = nullptr;
    ctx->PSSetShaderResources(0, 1, &nul);

    // ---- 3. Specular prefilter ----
    {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = cube_size; d.Height = cube_size;
        d.MipLevels = pref_mips; d.ArraySize = 6;
        d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        dev.d3d()->CreateTexture2D(&d, nullptr, pref_tex_.GetAddressOf());
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = d.Format;
        sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sv.TextureCube.MipLevels = pref_mips;
        dev.d3d()->CreateShaderResourceView(pref_tex_.Get(), &sv, pref_srv_.GetAddressOf());
    }
    ctx->VSSetShader(vs_face.Get(), nullptr, 0);
    ctx->PSSetShader(ps_pref.Get(), nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &env_for_use);
    for (u32 mip = 0; mip < pref_mips; ++mip) {
        u32 sz = std::max(1u, cube_size >> mip);
        f32 r = (pref_mips > 1) ? static_cast<f32>(mip) / static_cast<f32>(pref_mips - 1) : 0.0f;
        for (u32 face = 0; face < 6; ++face) {
            Com<ID3D11RenderTargetView> rtv;
            rtv_for_face(pref_tex_.Get(), face, mip, rtv);
            ID3D11RenderTargetView* rr = rtv.Get();
            ctx->OMSetRenderTargets(1, &rr, nullptr);
            set_viewport(sz, sz);
            CBIBL d{};
            face_view_proj(face, d.face_view, d.face_proj);
            d.face = face; d.size = sz; d.roughness = r;
            cb.update(dev, &d, sizeof(d));
            ID3D11Buffer* cbp = cb.d3d();
            ctx->VSSetConstantBuffers(0, 1, &cbp);
            ctx->PSSetConstantBuffers(0, 1, &cbp);
            draw_fullscreen();
        }
    }
    ctx->PSSetShaderResources(0, 1, &nul);

    // ---- 4. BRDF LUT ----
    {
        D3D11_TEXTURE2D_DESC d{};
        d.Width = 256; d.Height = 256; d.MipLevels = 1; d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R16G16_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        dev.d3d()->CreateTexture2D(&d, nullptr, brdf_tex_.GetAddressOf());
        Com<ID3D11RenderTargetView> rtv;
        dev.d3d()->CreateRenderTargetView(brdf_tex_.Get(), nullptr, rtv.GetAddressOf());
        dev.d3d()->CreateShaderResourceView(brdf_tex_.Get(), nullptr, brdf_srv_.GetAddressOf());
        ID3D11RenderTargetView* r = rtv.Get();
        ctx->OMSetRenderTargets(1, &r, nullptr);
        set_viewport(256, 256);
        ctx->VSSetShader(vs_face.Get(), nullptr, 0);
        ctx->PSSetShader(ps_brdf.Get(), nullptr, 0);
        CBIBL d2{};
        d2.size = 256;
        cb.update(dev, &d2, sizeof(d2));
        ID3D11Buffer* cbp = cb.d3d();
        ctx->VSSetConstantBuffers(0, 1, &cbp);
        ctx->PSSetConstantBuffers(0, 1, &cbp);
        draw_fullscreen();
    }

    // Restore.
    ctx->OMSetRenderTargets(0, nullptr, nullptr);
    return true;
}

void IBL::destroy() {
    brdf_srv_.Reset(); brdf_tex_.Reset();
    pref_srv_.Reset(); pref_tex_.Reset();
    irr_srv_.Reset();  irr_tex_.Reset();
    env_srv_.Reset();  env_tex_.Reset();
}

} // namespace cn::gfx
