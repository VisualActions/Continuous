#include "continuous/asset/AssetManager.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace cn::asset {

bool import_texture_file(const std::filesystem::path& src,
                         std::vector<u8>& pixels, u32& w, u32& h,
                         gfx::TextureFormat& format)
{
    int x = 0, y = 0, comp = 0;
    auto bytes = io::read_file_bytes(src);
    if (!bytes) return false;
    stbi_uc* data = stbi_load_from_memory(bytes->data(), static_cast<int>(bytes->size()),
                                          &x, &y, &comp, 4);
    if (!data) {
        CN_ERROR("asset", "stbi failed for {}: {}", src.string(), stbi_failure_reason());
        return false;
    }
    w = static_cast<u32>(x);
    h = static_cast<u32>(y);
    format = gfx::TextureFormat::SRGBA8;
    pixels.assign(data, data + (4 * x * y));
    stbi_image_free(data);
    return true;
}

} // namespace cn::asset
