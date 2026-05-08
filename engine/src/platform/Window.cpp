#include "continuous/platform/Window.h"
#include "continuous/platform/Input.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <SDL3/SDL.h>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#endif

namespace cn::platform {

static bool s_sdl_inited = false;

static bool ensure_sdl() {
    if (s_sdl_inited) return true;
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC)) {
        CN_ERROR("platform", "SDL_Init failed: {}", SDL_GetError());
        return false;
    }
    s_sdl_inited = true;
    return true;
}

Window::~Window() { destroy(); }

bool Window::create(const WindowDesc& desc) {
    desc_ = desc;
    if (!ensure_sdl()) return false;

    SDL_WindowFlags flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (desc.resizable) flags |= SDL_WINDOW_RESIZABLE;
    if (desc.maximized) flags |= SDL_WINDOW_MAXIMIZED;
    if (desc.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;

    sdl_ = SDL_CreateWindow(desc.title.c_str(),
                            static_cast<int>(desc.width),
                            static_cast<int>(desc.height),
                            flags);
    if (!sdl_) {
        CN_ERROR("platform", "SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }

    int w = 0, h = 0;
    SDL_GetWindowSize(sdl_, &w, &h);
    width_  = static_cast<u32>(w);
    height_ = static_cast<u32>(h);

#if defined(_WIN32)
    if (desc.dark_titlebar) {
        HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_),
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    }
#endif

    SDL_ShowWindow(sdl_);
    SDL_StartTextInput(sdl_);
    CN_INFO("platform", "window created {}x{} '{}'", width_, height_, desc.title);
    return true;
}

void Window::destroy() {
    if (sdl_) {
        SDL_DestroyWindow(sdl_);
        sdl_ = nullptr;
    }
}

void* Window::native_handle() const {
#if defined(_WIN32)
    if (!sdl_) return nullptr;
    return SDL_GetPointerProperty(SDL_GetWindowProperties(sdl_),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#else
    return nullptr;
#endif
}

void Window::set_title(const std::string& t) {
    desc_.title = t;
    if (sdl_) SDL_SetWindowTitle(sdl_, t.c_str());
}

void Window::set_capture_mouse(bool v) {
    capture_mouse_ = v;
    SDL_SetWindowRelativeMouseMode(sdl_, v);
}

bool Window::process_events() {
    auto& input = Input::get();
    input.begin_frame();
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (event_cb_) event_cb_(&ev);
        input.on_sdl_event(&ev);

        switch (ev.type) {
            case SDL_EVENT_QUIT:
                should_close_ = true;
                break;
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                if (sdl_ && SDL_GetWindowID(sdl_) == ev.window.windowID) should_close_ = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = 0, h = 0;
                SDL_GetWindowSize(sdl_, &w, &h);
                if ((u32)w != width_ || (u32)h != height_) {
                    width_   = (u32)w;
                    height_  = (u32)h;
                    resized_ = true;
                }
            } break;
            default: break;
        }
    }
    return !should_close_;
}

void Window::swap() {
    // The renderer (D3D11) presents via swapchain. SDL3 does not own present.
    // This hook is here for future hookups (e.g. ImGui frame flip).
}

} // namespace cn::platform
