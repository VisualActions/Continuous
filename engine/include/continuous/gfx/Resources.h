// Common GPU resource types: Buffer, Texture2D, RenderTexture, Sampler.
#pragma once

#include "continuous/gfx/Device.h"

#include <span>
#include <vector>

namespace cn::gfx {

enum class BufferType : u8 { Vertex, Index, Constant, Structured, Staging };
enum class TextureFormat : u16 {
    Unknown = 0,
    R8,
    RG8,
    RGBA8,
    SRGBA8,
    BC1,
    BC3,
    BC5,
    BC7,
    R16F,
    RG16F,
    RGBA16F,
    R32F,
    D32F,
    D24S8
};

DXGI_FORMAT  to_dxgi(TextureFormat f);
u32          format_bpp(TextureFormat f);

class CN_API Buffer {
public:
    Buffer() = default;
    ~Buffer() { destroy(); }
    CN_NONCOPYABLE(Buffer);

    bool create(Device& dev, BufferType type, usize size_bytes, const void* initial = nullptr,
                u32 stride = 0, bool dynamic = false);
    void destroy();

    void update(Device& dev, const void* data, usize size_bytes, usize offset = 0);

    ID3D11Buffer* d3d() const { return buf_.Get(); }
    BufferType    type() const { return type_; }
    usize         size() const { return size_; }
    u32           stride() const { return stride_; }

private:
    Com<ID3D11Buffer> buf_;
    BufferType        type_   = BufferType::Vertex;
    usize             size_   = 0;
    u32               stride_ = 0;
    bool              dynamic_= false;
};

struct CN_API Texture2DDesc {
    u32           width  = 1;
    u32           height = 1;
    u32           mips   = 1;
    u32           array_size = 1;
    TextureFormat format = TextureFormat::RGBA8;
    bool          is_cube = false;
    bool          render_target = false;
    bool          uav = false;
};

class CN_API Texture2D {
public:
    Texture2D() = default;
    ~Texture2D() { destroy(); }
    CN_NONCOPYABLE(Texture2D);

    bool create(Device& dev, const Texture2DDesc& desc, const void* initial_pixels = nullptr,
                u32 row_pitch = 0);
    void destroy();

    ID3D11Texture2D*           tex()  const { return tex_.Get(); }
    ID3D11ShaderResourceView*  srv()  const { return srv_.Get(); }
    ID3D11RenderTargetView*    rtv()  const { return rtv_.Get(); }
    ID3D11DepthStencilView*    dsv()  const { return dsv_.Get(); }
    ID3D11UnorderedAccessView* uav()  const { return uav_.Get(); }

    const Texture2DDesc& desc() const { return desc_; }

private:
    Com<ID3D11Texture2D>           tex_;
    Com<ID3D11ShaderResourceView>  srv_;
    Com<ID3D11RenderTargetView>    rtv_;
    Com<ID3D11DepthStencilView>    dsv_;
    Com<ID3D11UnorderedAccessView> uav_;
    Texture2DDesc                  desc_;
};

class CN_API Sampler {
public:
    static ID3D11SamplerState* linear_wrap();
    static ID3D11SamplerState* linear_clamp();
    static ID3D11SamplerState* point_clamp();
    static ID3D11SamplerState* anisotropic_wrap();
    static ID3D11SamplerState* shadow_compare();
    static void initialize(Device& dev);
    static void shutdown();
};

} // namespace cn::gfx
