#include "continuous/gfx/Resources.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <cstring>

namespace cn::gfx {

DXGI_FORMAT to_dxgi(TextureFormat f) {
    switch (f) {
        case TextureFormat::R8:      return DXGI_FORMAT_R8_UNORM;
        case TextureFormat::RG8:     return DXGI_FORMAT_R8G8_UNORM;
        case TextureFormat::RGBA8:   return DXGI_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::SRGBA8:  return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case TextureFormat::BC1:     return DXGI_FORMAT_BC1_UNORM;
        case TextureFormat::BC3:     return DXGI_FORMAT_BC3_UNORM;
        case TextureFormat::BC5:     return DXGI_FORMAT_BC5_UNORM;
        case TextureFormat::BC7:     return DXGI_FORMAT_BC7_UNORM;
        case TextureFormat::R16F:    return DXGI_FORMAT_R16_FLOAT;
        case TextureFormat::RG16F:   return DXGI_FORMAT_R16G16_FLOAT;
        case TextureFormat::RGBA16F: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case TextureFormat::R32F:    return DXGI_FORMAT_R32_FLOAT;
        case TextureFormat::D32F:    return DXGI_FORMAT_D32_FLOAT;
        case TextureFormat::D24S8:   return DXGI_FORMAT_D24_UNORM_S8_UINT;
        default:                     return DXGI_FORMAT_UNKNOWN;
    }
}

u32 format_bpp(TextureFormat f) {
    switch (f) {
        case TextureFormat::R8: return 1;
        case TextureFormat::RG8: return 2;
        case TextureFormat::RGBA8:
        case TextureFormat::SRGBA8: return 4;
        case TextureFormat::R16F:   return 2;
        case TextureFormat::RG16F:  return 4;
        case TextureFormat::RGBA16F:return 8;
        case TextureFormat::R32F:   return 4;
        case TextureFormat::D32F:   return 4;
        case TextureFormat::D24S8:  return 4;
        default: return 4;
    }
}

// ----------------------------------------------------------------------------
// Buffer
// ----------------------------------------------------------------------------
bool Buffer::create(Device& dev, BufferType type, usize size, const void* initial,
                    u32 stride, bool dynamic) {
    type_   = type;
    size_   = size;
    stride_ = stride;
    dynamic_= dynamic;

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(size);
    desc.Usage     = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;

    switch (type) {
        case BufferType::Vertex:    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;   break;
        case BufferType::Index:     desc.BindFlags = D3D11_BIND_INDEX_BUFFER;    break;
        case BufferType::Constant: {
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            // CB sizes must be multiples of 16.
            desc.ByteWidth = static_cast<UINT>((size + 15) & ~15ull);
            size_ = desc.ByteWidth;
            dynamic_ = true;
        } break;
        case BufferType::Structured:
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            desc.StructureByteStride = stride;
            break;
        case BufferType::Staging:
            desc.Usage = D3D11_USAGE_STAGING;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            break;
    }

    D3D11_SUBRESOURCE_DATA srd{};
    if (initial) {
        srd.pSysMem = initial;
        srd.SysMemPitch = static_cast<UINT>(size);
    }
    HRESULT hr = dev.d3d()->CreateBuffer(&desc, initial ? &srd : nullptr, buf_.GetAddressOf());
    if (FAILED(hr)) {
        CN_ERROR("gfx", "CreateBuffer failed 0x{:08x}", static_cast<u32>(hr));
        return false;
    }
    return true;
}

void Buffer::destroy() { buf_.Reset(); size_ = 0; }

void Buffer::update(Device& dev, const void* data, usize size, usize offset) {
    if (!buf_) return;
    if (dynamic_) {
        D3D11_MAPPED_SUBRESOURCE m{};
        if (SUCCEEDED(dev.context()->Map(buf_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
            std::memcpy(static_cast<u8*>(m.pData) + offset, data, size);
            dev.context()->Unmap(buf_.Get(), 0);
        }
    } else {
        // For default usage, do a partial UpdateSubresource.
        D3D11_BOX box{};
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + size);
        box.top = 0; box.bottom = 1; box.front = 0; box.back = 1;
        dev.context()->UpdateSubresource(buf_.Get(), 0, &box, data, 0, 0);
    }
}

// ----------------------------------------------------------------------------
// Texture2D
// ----------------------------------------------------------------------------
bool Texture2D::create(Device& dev, const Texture2DDesc& desc, const void* pixels, u32 row_pitch) {
    desc_ = desc;

    D3D11_TEXTURE2D_DESC d{};
    d.Width  = desc.width;
    d.Height = desc.height;
    d.MipLevels = desc.mips;
    d.ArraySize = desc.is_cube ? 6 * desc.array_size : desc.array_size;
    d.Format = to_dxgi(desc.format);
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (desc.render_target) {
        if (desc.format == TextureFormat::D32F || desc.format == TextureFormat::D24S8)
            d.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        else
            d.BindFlags |= D3D11_BIND_RENDER_TARGET;
    }
    if (desc.uav) d.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    if (desc.is_cube) d.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
    if (desc.mips != 1 && desc.render_target)
        d.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

    std::vector<D3D11_SUBRESOURCE_DATA> srd;
    if (pixels && desc.mips == 1 && !desc.is_cube && desc.array_size == 1) {
        srd.resize(1);
        srd[0].pSysMem = pixels;
        srd[0].SysMemPitch = row_pitch ? row_pitch : (desc.width * format_bpp(desc.format));
    }
    HRESULT hr = dev.d3d()->CreateTexture2D(&d, srd.empty() ? nullptr : srd.data(), tex_.GetAddressOf());
    if (FAILED(hr)) {
        CN_ERROR("gfx", "CreateTexture2D failed 0x{:08x}", static_cast<u32>(hr));
        return false;
    }

    if (d.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
        sv.Format = d.Format;
        if (desc.format == TextureFormat::D32F)        sv.Format = DXGI_FORMAT_R32_FLOAT;
        else if (desc.format == TextureFormat::D24S8)  sv.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        if (desc.is_cube) {
            sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            sv.TextureCube.MipLevels = desc.mips;
        } else if (desc.array_size > 1) {
            sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            sv.Texture2DArray.ArraySize = desc.array_size;
            sv.Texture2DArray.MipLevels = desc.mips;
        } else {
            sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sv.Texture2D.MipLevels = desc.mips;
        }
        // For depth, recreate desc so that we sample it as the colour.
        if (desc.format == TextureFormat::D32F || desc.format == TextureFormat::D24S8) {
            D3D11_TEXTURE2D_DESC dd = d;
            dd.Format = (desc.format == TextureFormat::D32F)
                ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_R24G8_TYPELESS;
            // Recreate texture with typeless format for SRV+DSV pairing.
            tex_.Reset();
            hr = dev.d3d()->CreateTexture2D(&dd, nullptr, tex_.GetAddressOf());
            if (FAILED(hr)) return false;
            sv.Format = (desc.format == TextureFormat::D32F)
                ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        }
        dev.d3d()->CreateShaderResourceView(tex_.Get(), &sv, srv_.GetAddressOf());
    }

    if (d.BindFlags & D3D11_BIND_RENDER_TARGET) {
        D3D11_RENDER_TARGET_VIEW_DESC rt{};
        rt.Format = d.Format;
        rt.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        dev.d3d()->CreateRenderTargetView(tex_.Get(), &rt, rtv_.GetAddressOf());
    }
    if (d.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
        D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
        dv.Format = (desc.format == TextureFormat::D32F)
            ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dev.d3d()->CreateDepthStencilView(tex_.Get(), &dv, dsv_.GetAddressOf());
    }
    if (d.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uv{};
        uv.Format = d.Format;
        uv.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
        dev.d3d()->CreateUnorderedAccessView(tex_.Get(), &uv, uav_.GetAddressOf());
    }

    if (desc.mips != 1 && desc.render_target && srv_) {
        dev.context()->GenerateMips(srv_.Get());
    }
    return true;
}

void Texture2D::destroy() {
    uav_.Reset();
    dsv_.Reset();
    rtv_.Reset();
    srv_.Reset();
    tex_.Reset();
}

// ----------------------------------------------------------------------------
// Sampler
// ----------------------------------------------------------------------------
namespace {
struct SamplerState {
    Com<ID3D11SamplerState> linear_wrap;
    Com<ID3D11SamplerState> linear_clamp;
    Com<ID3D11SamplerState> point_clamp;
    Com<ID3D11SamplerState> aniso_wrap;
    Com<ID3D11SamplerState> shadow_compare;
} g_samplers;
}

void Sampler::initialize(Device& dev) {
    auto make = [&](D3D11_SAMPLER_DESC d, ID3D11SamplerState** out) {
        dev.d3d()->CreateSamplerState(&d, out);
    };
    D3D11_SAMPLER_DESC s{};
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    make(s, g_samplers.linear_wrap.GetAddressOf());

    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    make(s, g_samplers.linear_clamp.GetAddressOf());

    s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    make(s, g_samplers.point_clamp.GetAddressOf());

    D3D11_SAMPLER_DESC a{};
    a.Filter = D3D11_FILTER_ANISOTROPIC;
    a.MaxAnisotropy = 8;
    a.AddressU = a.AddressV = a.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    a.MaxLOD = D3D11_FLOAT32_MAX;
    make(a, g_samplers.aniso_wrap.GetAddressOf());

    D3D11_SAMPLER_DESC sh{};
    sh.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    sh.AddressU = sh.AddressV = sh.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    sh.BorderColor[0] = sh.BorderColor[1] = sh.BorderColor[2] = sh.BorderColor[3] = 1.0f;
    sh.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    sh.MaxLOD = D3D11_FLOAT32_MAX;
    make(sh, g_samplers.shadow_compare.GetAddressOf());
}

void Sampler::shutdown() {
    g_samplers.linear_wrap.Reset();
    g_samplers.linear_clamp.Reset();
    g_samplers.point_clamp.Reset();
    g_samplers.aniso_wrap.Reset();
    g_samplers.shadow_compare.Reset();
}

ID3D11SamplerState* Sampler::linear_wrap()      { return g_samplers.linear_wrap.Get(); }
ID3D11SamplerState* Sampler::linear_clamp()     { return g_samplers.linear_clamp.Get(); }
ID3D11SamplerState* Sampler::point_clamp()      { return g_samplers.point_clamp.Get(); }
ID3D11SamplerState* Sampler::anisotropic_wrap() { return g_samplers.aniso_wrap.Get(); }
ID3D11SamplerState* Sampler::shadow_compare()   { return g_samplers.shadow_compare.Get(); }

} // namespace cn::gfx
