// Continuous Engine - Window abstraction over SDL3.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/math/Math.h"

#include <functional>
#include <string>

struct SDL_Window;

namespace cn::platform {

struct WindowDesc {
    std::string title{"Continuous"};
    u32  width{1600};
    u32  height{900};
    bool resizable{true};
    bool maximized{false};
    bool fullscreen{false};
    bool vsync{true};
    bool dark_titlebar{true};
};

class CN_API Window {
public:
    Window() = default;
    ~Window();
    CN_NONCOPYABLE(Window);

    bool create(const WindowDesc& desc);
    void destroy();

    bool process_events();   // returns false on quit
    void swap();             // present (no-op here, the renderer owns present)

    void* native_handle() const;   // HWND
    SDL_Window* sdl_window() const { return sdl_; }

    u32 width()  const { return width_; }
    u32 height() const { return height_; }
    f32 aspect() const { return height_ ? static_cast<f32>(width_) / static_cast<f32>(height_) : 1.0f; }

    bool should_close() const { return should_close_; }
    void set_should_close(bool v) { should_close_ = v; }

    bool was_resized()  const { return resized_; }
    void clear_resized()      { resized_ = false; }

    void set_title(const std::string& t);
    const std::string& title() const { return desc_.title; }

    void set_capture_mouse(bool v);
    bool capture_mouse() const { return capture_mouse_; }

    using EventCallback = std::function<void(void*)>; // SDL_Event*
    void set_event_callback(EventCallback cb) { event_cb_ = std::move(cb); }

private:
    SDL_Window*    sdl_ = nullptr;
    WindowDesc     desc_{};
    u32            width_  = 0;
    u32            height_ = 0;
    bool           should_close_  = false;
    bool           resized_       = false;
    bool           capture_mouse_ = false;
    EventCallback  event_cb_;
};

} // namespace cn::platform
