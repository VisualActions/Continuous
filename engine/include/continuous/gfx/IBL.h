// Image-Based Lighting: environment cubemap + irradiance + specular prefilter
// + BRDF LUT for the split-sum approximation.
//
// In this build the cubemap is generated from a procedural sky shader so we
// don't ship a giant HDR equirectangular file - good enough for the sandbox
// to demonstrate IBL in PBR. A proper engine would also load equirect HDR.
#pragma once

#include "continuous/gfx/Resources.h"

namespace cn::gfx {

class CN_API IBL {
public:
    IBL() = default;
    ~IBL() = default;
    CN_NONCOPYABLE(IBL);

    bool generate_procedural(Device& dev, u32 cube_size = 256, u32 irradiance_size = 32,
                             u32 prefilter_mips = 5);
    void destroy();

    ID3D11ShaderResourceView* environment_srv()  const { return env_srv_.Get(); }
    ID3D11ShaderResourceView* irradiance_srv()   const { return irr_srv_.Get(); }
    ID3D11ShaderResourceView* prefilter_srv()    const { return pref_srv_.Get(); }
    ID3D11ShaderResourceView* brdf_lut_srv()     const { return brdf_srv_.Get(); }
    u32  prefilter_mip_count() const { return prefilter_mips_; }

private:
    Com<ID3D11Texture2D>          env_tex_;
    Com<ID3D11ShaderResourceView> env_srv_;
    Com<ID3D11Texture2D>          irr_tex_;
    Com<ID3D11ShaderResourceView> irr_srv_;
    Com<ID3D11Texture2D>          pref_tex_;
    Com<ID3D11ShaderResourceView> pref_srv_;
    Com<ID3D11Texture2D>          brdf_tex_;
    Com<ID3D11ShaderResourceView> brdf_srv_;
    u32                           prefilter_mips_ = 5;
};

} // namespace cn::gfx
