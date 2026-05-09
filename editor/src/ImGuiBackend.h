// Tiny wrapper around the ImGui SDL3 + D3D11 backends so EditorApp does not
// need to know the headers.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/gfx/Device.h"
#include "continuous/platform/Window.h"

namespace cnedit {

bool imgui_init   (cn::platform::Window& w, cn::gfx::Device& d);
void imgui_shutdown();
void imgui_new_frame();
void imgui_handle_event(void* sdl_event);
void imgui_render(cn::gfx::Device& d, ID3D11RenderTargetView* rtv);

} // namespace cnedit
