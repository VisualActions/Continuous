#include "continuous/gfx/ShaderCompiler.h"
#include "continuous/core/Assert.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <fstream>
#include <unordered_map>

#pragma comment(lib, "d3dcompiler.lib")

namespace cn::gfx {

const char* stage_target(ShaderStage s) {
    switch (s) {
        case ShaderStage::Vertex:   return "vs_5_0";
        case ShaderStage::Pixel:    return "ps_5_0";
        case ShaderStage::Compute:  return "cs_5_0";
        case ShaderStage::Geometry: return "gs_5_0";
        case ShaderStage::Hull:     return "hs_5_0";
        case ShaderStage::Domain:   return "ds_5_0";
    }
    return "vs_5_0";
}

// Custom #include handler: resolves relative to the shaders root.
class IncludeHandler final : public ID3DInclude {
public:
    explicit IncludeHandler(std::filesystem::path root) : root_(std::move(root)) {}
    HRESULT __stdcall Open(D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID,
                           LPCVOID* ppData, UINT* pBytes) override {
        std::filesystem::path full = root_ / pFileName;
        std::ifstream f(full, std::ios::binary | std::ios::ate);
        if (!f) return E_FAIL;
        auto sz = static_cast<usize>(f.tellg());
        char* mem = new char[sz];
        f.seekg(0);
        f.read(mem, sz);
        *ppData = mem;
        *pBytes = static_cast<UINT>(sz);
        return S_OK;
    }
    HRESULT __stdcall Close(LPCVOID pData) override {
        delete[] static_cast<const char*>(pData);
        return S_OK;
    }
private:
    std::filesystem::path root_;
};

ShaderCompiler& ShaderCompiler::get() {
    static ShaderCompiler c;
    return c;
}

CompiledShader ShaderCompiler::compile_source(const std::string& source,
                                              const char* file_id,
                                              const char* entry,
                                              ShaderStage stage,
                                              const std::vector<ShaderMacro>& defines) {
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#if defined(_DEBUG) || !defined(NDEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    std::vector<D3D_SHADER_MACRO> macs;
    macs.reserve(defines.size() + 1);
    for (auto& m : defines) macs.push_back({ m.name.c_str(), m.value.c_str() });
    macs.push_back({ nullptr, nullptr });

    Microsoft::WRL::ComPtr<ID3DBlob> code, err;
    IncludeHandler inc(shaders_root_.empty() ? std::filesystem::current_path() : shaders_root_);

    HRESULT hr = D3DCompile(
        source.data(), source.size(), file_id,
        macs.data(), &inc,
        entry, stage_target(stage),
        flags, 0,
        code.GetAddressOf(), err.GetAddressOf());

    CompiledShader out;
    if (FAILED(hr)) {
        if (err) out.error = std::string(static_cast<const char*>(err->GetBufferPointer()), err->GetBufferSize());
        else     out.error = "D3DCompile failed";
        out.ok = false;
        CN_ERROR("gfx", "shader compile failed for {}@{}: {}", file_id ? file_id : "<src>", entry, out.error);
        return out;
    }
    out.bytecode.assign(static_cast<u8*>(code->GetBufferPointer()),
                        static_cast<u8*>(code->GetBufferPointer()) + code->GetBufferSize());
    out.ok = true;
    return out;
}

CompiledShader ShaderCompiler::compile_file(const std::filesystem::path& path,
                                            const char* entry,
                                            ShaderStage stage,
                                            const std::vector<ShaderMacro>& defines) {
    std::filesystem::path resolved = path.is_absolute() ? path : (shaders_root_ / path);
    auto src = io::read_file_text(resolved);
    if (!src) {
        CompiledShader cs;
        cs.error = "shader file not found: " + resolved.string();
        cs.ok = false;
        CN_ERROR("gfx", "{}", cs.error);
        return cs;
    }
    auto file_id = resolved.filename().string();
    return compile_source(*src, file_id.c_str(), entry, stage, defines);
}

} // namespace cn::gfx
