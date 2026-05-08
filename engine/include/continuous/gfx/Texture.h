// Texture loading + cooked .ctex format readers. The actual import (PNG/JPG/HDR
// decode via stb_image) lives in asset/TextureImport. This header only owns
// the runtime Texture type, which is a Texture2D plus a label.
#pragma once

#include "continuous/gfx/Resources.h"

#include <string>

namespace cn::gfx {

class CN_API Texture {
public:
    Texture() = default;
    ~Texture() = default;
    CN_NONCOPYABLE(Texture);

    bool load_from_pixels(Device& dev, u32 w, u32 h, TextureFormat fmt,
                          const void* pixels, u32 row_pitch = 0, bool gen_mips = true);

    Texture2D& tex2d() { return tex_; }
    const Texture2D& tex2d() const { return tex_; }
    ID3D11ShaderResourceView* srv() const { return tex_.srv(); }

    void set_label(std::string s) { label_ = std::move(s); }
    const std::string& label() const { return label_; }

private:
    Texture2D   tex_;
    std::string label_;
};

// Returns a 1x1 white texture useful as a default albedo / metallic etc.
CN_API Texture* white_pixel();
CN_API Texture* black_pixel();
CN_API Texture* normal_default(); // (0.5, 0.5, 1, 1)
CN_API void     init_defaults(Device& dev);
CN_API void     shutdown_defaults();

} // namespace cn::gfx
