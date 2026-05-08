#include "continuous/gfx/Device.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace cn::gfx {

Device::~Device() { destroy(); }

bool Device::create(bool enable_debug_layer) {
    UINT flags = 0;
    if (enable_debug_layer) flags |= D3D11_CREATE_DEVICE_DEBUG;

    D3D_FEATURE_LEVEL levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };
    D3D_FEATURE_LEVEL got{};
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        flags, levels, _countof(levels), D3D11_SDK_VERSION,
        device_.GetAddressOf(), &got, context_.GetAddressOf());

    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
        // Debug layer probably not installed - retry without it.
        CN_WARN("gfx", "D3D11 debug layer unavailable, retrying without");
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            flags, levels, _countof(levels), D3D11_SDK_VERSION,
            device_.GetAddressOf(), &got, context_.GetAddressOf());
    }
    if (FAILED(hr)) {
        CN_ERROR("gfx", "D3D11CreateDevice failed: 0x{:08x}", static_cast<u32>(hr));
        return false;
    }

    // DXGI factory and tearing capability.
    Com<IDXGIDevice>  dxgi_dev;
    Com<IDXGIAdapter> adapter;
    device_.As(&dxgi_dev);
    dxgi_dev->GetAdapter(adapter.GetAddressOf());
    adapter->GetParent(IID_PPV_ARGS(factory_.GetAddressOf()));

    BOOL allow_tearing = FALSE;
    factory_->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                  &allow_tearing, sizeof(allow_tearing));
    tearing_supported_ = allow_tearing == TRUE;

    if (enable_debug_layer) {
        device_.As(&debug_);
    }

    DXGI_ADAPTER_DESC desc{};
    adapter->GetDesc(&desc);
    char name8[256] = {};
    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, name8, sizeof(name8), nullptr, nullptr);
    CN_INFO("gfx", "D3D11 created: {} (FL={}.{}{}",
            name8,
            (got >> 12) & 0xF, (got >> 8) & 0xF,
            tearing_supported_ ? ", tearing)" : ")");
    return true;
}

void Device::destroy() {
    debug_.Reset();
    factory_.Reset();
    context_.Reset();
    device_.Reset();
}

void Device::name(ID3D11DeviceChild* res, const char* nm) {
    if (!res || !nm) return;
    res->SetPrivateData(WKPDID_D3DDebugObjectName,
                        static_cast<UINT>(strlen(nm)), nm);
}

Device& global_device() {
    static Device d;
    return d;
}

} // namespace cn::gfx
