#include "ImGuiBackend.h"

#include <SDL3/SDL.h>

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_dx11.h>

namespace cnedit {

bool imgui_init(cn::platform::Window& w, cn::gfx::Device& d) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowRounding = 4.0f;
    s.FrameRounding  = 3.0f;
    s.ScrollbarRounding = 3.0f;
    s.GrabRounding   = 3.0f;
    if (!ImGui_ImplSDL3_InitForD3D(w.sdl_window())) return false;
    if (!ImGui_ImplDX11_Init(d.d3d(), d.context()))  return false;
    return true;
}

void imgui_shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void imgui_new_frame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void imgui_handle_event(void* ev) {
    ImGui_ImplSDL3_ProcessEvent(static_cast<SDL_Event*>(ev));
}

void imgui_render(cn::gfx::Device& d, ID3D11RenderTargetView* rtv) {
    ImGui::Render();
    ID3D11RenderTargetView* rtvs[1] = { rtv };
    d.context()->OMSetRenderTargets(1, rtvs, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

} // namespace cnedit
