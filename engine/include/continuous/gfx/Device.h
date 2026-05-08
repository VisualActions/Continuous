// Continuous Engine - D3D11 device wrapper.
//
// Why D3D11: ships fast, debug-friendly (PIX/RenderDoc work great), perfectly
// adequate for the visual targets we care about (PBR, shadows, HDR/IBL, post
// FX). D3D12 would be more code without buying us anything for this project's
// scope.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

#include <wrl/client.h>
#include <d3d11.h>
#include <dxgi1_5.h>

namespace cn::gfx {

template <typename T> using Com = Microsoft::WRL::ComPtr<T>;

class CN_API Device {
public:
    Device() = default;
    ~Device();
    CN_NONCOPYABLE(Device);

    bool create(bool enable_debug_layer);
    void destroy();

    ID3D11Device*        d3d()     const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    IDXGIFactory5*       factory() const { return factory_.Get(); }

    bool supports_tearing() const { return tearing_supported_; }

    // Util: name a D3D resource for PIX / GPU debugger.
    static void name(ID3D11DeviceChild* res, const char* name);

private:
    Com<ID3D11Device>        device_;
    Com<ID3D11DeviceContext> context_;
    Com<IDXGIFactory5>       factory_;
    Com<ID3D11Debug>         debug_;
    bool                     tearing_supported_ = false;
};

CN_API Device& global_device();

} // namespace cn::gfx
