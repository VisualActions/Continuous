#include "continuous/gfx/SwapChain.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

namespace cn::gfx {

SwapChain::~SwapChain() { destroy(); }

bool SwapChain::create(Device& dev, void* hwnd, u32 w, u32 h, bool vsync) {
    dev_     = &dev;
    width_   = w;
    height_  = h;
    vsync_   = vsync;
    tearing_ = dev.supports_tearing();
    format_  = DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width  = w;
    desc.Height = h;
    desc.Format = format_;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode  = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    HRESULT hr = dev.factory()->CreateSwapChainForHwnd(
        dev.d3d(), static_cast<HWND>(hwnd), &desc, nullptr, nullptr, sc_.GetAddressOf());
    if (FAILED(hr)) {
        CN_ERROR("gfx", "CreateSwapChainForHwnd failed: 0x{:08x}", static_cast<u32>(hr));
        return false;
    }
    dev.factory()->MakeWindowAssociation(static_cast<HWND>(hwnd), DXGI_MWA_NO_ALT_ENTER);

    create_views_();
    return true;
}

void SwapChain::create_views_() {
    sc_->GetBuffer(0, IID_PPV_ARGS(back_buffer_.GetAddressOf()));
    dev_->d3d()->CreateRenderTargetView(back_buffer_.Get(), nullptr, rtv_.GetAddressOf());
    Device::name(back_buffer_.Get(), "swap_back_buffer");
    Device::name(rtv_.Get(), "swap_back_buffer_rtv");
}

void SwapChain::destroy() {
    rtv_.Reset();
    back_buffer_.Reset();
    sc_.Reset();
    dev_ = nullptr;
}

void SwapChain::resize(u32 w, u32 h) {
    if (!sc_) return;
    if (w == 0 || h == 0) return;
    rtv_.Reset();
    back_buffer_.Reset();
    HRESULT hr = sc_->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN,
                                    tearing_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    if (FAILED(hr)) {
        CN_ERROR("gfx", "swap ResizeBuffers failed 0x{:08x}", static_cast<u32>(hr));
        return;
    }
    width_ = w; height_ = h;
    create_views_();
}

void SwapChain::present() {
    if (!sc_) return;
    UINT flags = (!vsync_ && tearing_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
    UINT sync  = vsync_ ? 1 : 0;
    sc_->Present(sync, flags);
}

} // namespace cn::gfx
