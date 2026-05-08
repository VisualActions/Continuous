// Online HLSL compilation via D3DCompile. Supports basic #include resolution
// (file-relative), defines, and a small disk cache keyed on source hash + flags.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/gfx/Device.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace cn::gfx {

enum class ShaderStage : u8 { Vertex, Pixel, Compute, Geometry, Hull, Domain };

struct ShaderMacro {
    std::string name;
    std::string value;
};

struct CompiledShader {
    std::vector<u8> bytecode;
    bool            ok{false};
    std::string     error;
};

class CN_API ShaderCompiler {
public:
    static ShaderCompiler& get();

    // Compile from file path (relative to the engine's shaders root by default).
    CompiledShader compile_file(const std::filesystem::path& path,
                                const char* entry,
                                ShaderStage stage,
                                const std::vector<ShaderMacro>& defines = {});

    CompiledShader compile_source(const std::string& source,
                                  const char* file_id,
                                  const char* entry,
                                  ShaderStage stage,
                                  const std::vector<ShaderMacro>& defines = {});

    // Cache control
    void set_shaders_root(std::filesystem::path p) { shaders_root_ = std::move(p); }
    const std::filesystem::path& shaders_root() const { return shaders_root_; }

private:
    ShaderCompiler() = default;
    std::filesystem::path shaders_root_;
};

CN_API const char* stage_target(ShaderStage s);

} // namespace cn::gfx
