// Post-processing chain: bloom (downsample/upsample), tonemap (ACES), FXAA.
#pragma once

#include "continuous/gfx/Resources.h"

namespace cn::gfx {

class CN_API PostProcess {
public:
    PostProcess() = default;
    ~PostProcess() = default;
    CN_NONCOPYABLE(PostProcess);

    bool init(Device& dev);
    void destroy();

    void resize(Device& dev, u32 w, u32 h);

    // Reads HDR src SRV, writes to LDR rtv. Optionally writes the post chain
    // intermediate SRV to dest_srv_out.
    void run(Device& dev,
             ID3D11ShaderResourceView* hdr_src,
             ID3D11RenderTargetView*   ldr_dest,
             u32 w, u32 h);

    // Knobs.
    f32 bloom_threshold = 1.5f;
    f32 bloom_strength  = 0.5f;
    f32 exposure        = 1.0f;
    f32 gamma           = 2.2f;
    bool fxaa_enabled   = true;

private:
    Com<ID3D11VertexShader> vs_fullscreen_;
    Com<ID3D11PixelShader>  ps_threshold_;
    Com<ID3D11PixelShader>  ps_downsample_;
    Com<ID3D11PixelShader>  ps_upsample_;
    Com<ID3D11PixelShader>  ps_tonemap_;
    Com<ID3D11PixelShader>  ps_fxaa_;

    Buffer cb_post_;

    static constexpr u32 kBloomMips = 6;
    Com<ID3D11Texture2D>          bloom_tex_[kBloomMips];
    Com<ID3D11RenderTargetView>   bloom_rtv_[kBloomMips];
    Com<ID3D11ShaderResourceView> bloom_srv_[kBloomMips];
    u32                           bloom_w_[kBloomMips]{};
    u32                           bloom_h_[kBloomMips]{};
    u32                           rt_w_ = 0, rt_h_ = 0;

    Com<ID3D11Texture2D>          tone_tex_;
    Com<ID3D11RenderTargetView>   tone_rtv_;
    Com<ID3D11ShaderResourceView> tone_srv_;

    Com<ID3D11SamplerState> sampler_clamp_;
    Com<ID3D11BlendState>   bs_additive_;
    Com<ID3D11RasterizerState> rs_;
    Com<ID3D11DepthStencilState> ds_;
};

} // namespace cn::gfx
