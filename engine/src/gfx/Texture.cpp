#include "continuous/gfx/Texture.h"
#include "continuous/core/Log.h"

namespace cn::gfx {

bool Texture::load_from_pixels(Device& dev, u32 w, u32 h, TextureFormat fmt,
                               const void* pixels, u32 row_pitch, bool gen_mips) {
    Texture2DDesc d;
    d.width  = w;
    d.height = h;
    d.format = fmt;
    d.mips   = gen_mips ? 0 : 1; // 0 = full chain when render_target+srv
    d.render_target = gen_mips;  // needed for GenerateMips path
    d.is_cube = false;
    d.array_size = 1;

    // Without render target binding GenerateMips can't be used. We'll create
    // with one mip + initial pixels for the simple path; the gen_mips path
    // creates without initial pixels and uses a tiny stage upload.
    if (!gen_mips) {
        d.mips = 1;
        return tex_.create(dev, d, pixels, row_pitch);
    }

    d.mips = 0;
    if (!tex_.create(dev, d)) return false;
    // Upload mip 0.
    UINT pitch = row_pitch ? row_pitch : (w * format_bpp(fmt));
    dev.context()->UpdateSubresource(tex_.tex(), 0, nullptr, pixels, pitch, 0);
    if (tex_.srv()) dev.context()->GenerateMips(tex_.srv());
    return true;
}

namespace {
struct DefaultTextures {
    Texture white;
    Texture black;
    Texture normal;
    bool inited = false;
} g_def;
}

void init_defaults(Device& dev) {
    if (g_def.inited) return;
    g_def.inited = true;
    u8 white_px[4]  = { 255, 255, 255, 255 };
    u8 black_px[4]  = { 0,   0,   0,   255 };
    u8 normal_px[4] = { 128, 128, 255, 255 };
    g_def.white.load_from_pixels(dev, 1, 1, TextureFormat::RGBA8, white_px, 4, false);
    g_def.black.load_from_pixels(dev, 1, 1, TextureFormat::RGBA8, black_px, 4, false);
    g_def.normal.load_from_pixels(dev, 1, 1, TextureFormat::RGBA8, normal_px, 4, false);
    g_def.white.set_label("default_white");
    g_def.black.set_label("default_black");
    g_def.normal.set_label("default_normal");
}

void shutdown_defaults() {
    g_def = DefaultTextures{};
}

Texture* white_pixel()      { return &g_def.white; }
Texture* black_pixel()      { return &g_def.black; }
Texture* normal_default()   { return &g_def.normal; }

} // namespace cn::gfx
