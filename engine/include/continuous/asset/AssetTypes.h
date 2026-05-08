#pragma once

#include "continuous/core/Types.h"

namespace cn::asset {

enum class AssetKind : u32 {
    Unknown = 0,
    Mesh,
    Texture,
    Material,
    Audio,
    Scene,
};

constexpr u32 kCookedMagic = 'CNTC';
constexpr u32 kCookedMeshVersion    = 1;
constexpr u32 kCookedTextureVersion = 1;
constexpr u32 kCookedAudioVersion   = 1;

struct CookedHeader {
    u32 magic;
    u32 kind;
    u32 version;
    u32 size_bytes;
    u64 source_hash;
};

} // namespace cn::asset
