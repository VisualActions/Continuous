#include "continuous/asset/AssetManager.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#include <miniaudio.h>

#include <cstdio>
#include <cstring>

namespace cn::asset {

// Cooked audio format: header [magic 'CNAU' u32, channels u32, sample_rate u32,
// frame_count u32], then float32 LE PCM, interleaved.
struct AudioCookedHeader {
    u32 magic;
    u32 channels;
    u32 sample_rate;
    u32 frame_count;
};

constexpr u32 kAudioMagic = 'UANC';

bool Manager::cook_audio(const std::filesystem::path& src, const std::filesystem::path& dst) {
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_decoder dec;
    if (ma_decoder_init_file(src.string().c_str(), &cfg, &dec) != MA_SUCCESS) {
        CN_ERROR("asset", "ma_decoder_init_file failed for {}", src.string());
        return false;
    }
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&dec, &frames);
    if (frames == 0) {
        ma_decoder_uninit(&dec);
        CN_ERROR("asset", "audio is empty: {}", src.string());
        return false;
    }
    std::vector<f32> pcm(frames * dec.outputChannels);
    ma_uint64 read = 0;
    ma_decoder_read_pcm_frames(&dec, pcm.data(), frames, &read);
    AudioCookedHeader h{ kAudioMagic, dec.outputChannels,
                         (u32)dec.outputSampleRate, (u32)read };
    std::filesystem::create_directories(dst.parent_path());
    FILE* f = nullptr;
    fopen_s(&f, dst.string().c_str(), "wb");
    if (!f) { ma_decoder_uninit(&dec); return false; }
    std::fwrite(&h, 1, sizeof(h), f);
    std::fwrite(pcm.data(), sizeof(f32), pcm.size(), f);
    std::fclose(f);
    ma_decoder_uninit(&dec);
    return true;
}

} // namespace cn::asset
