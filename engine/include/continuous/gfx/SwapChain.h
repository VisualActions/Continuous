#pragma once

#include "continuous/gfx/Device.h"

namespace cn::gfx {

class CN_API SwapChain {
public:
    SwapChain() = default;
    ~SwapChain();
    CN_NONCOPYABLE(SwapChain);

    bool create(Device& dev, void* hwnd, u32 width, u32 height, bool vsync);
    void destroy();

    void resize(u32 width, u32 height);
    void present();

    ID3D11RenderTargetView* back_buffer_rtv() const { return rtv_.Get(); }
    ID3D11Texture2D*        back_buffer_tex() const { return back_buffer_.Get(); }
    DXGI_FORMAT             back_buffer_format() const { return format_; }

    u32 width()  const { return width_; }
    u32 height() const { return height_; }

    void set_vsync(bool v) { vsync_ = v; }
    bool vsync() const { return vsync_; }

private:
    void create_views_();

    Device*                   dev_ = nullptr;
    Com<IDXGISwapChain1>      sc_;
    Com<ID3D11Texture2D>      back_buffer_;
    Com<ID3D11RenderTargetView> rtv_;
    DXGI_FORMAT               format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    u32                       width_  = 0;
    u32                       height_ = 0;
    bool                      vsync_  = true;
    bool                      tearing_= false;
};

} // namespace cn::gfx
