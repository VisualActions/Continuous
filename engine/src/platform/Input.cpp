#include "continuous/platform/Input.h"
#include "continuous/core/Log.h"

#include <SDL3/SDL.h>

#include <cstring>

namespace cn::platform {

Input& Input::get() {
    static Input s;
    return s;
}

void Input::begin_frame() {
    key_pressed_.reset();
    key_released_.reset();
    mouse_pressed_.reset();
    mouse_released_.reset();
    mouse_delta_ = math::vec2(0);
    wheel_       = 0.0f;
    text_input_.clear();
    for (auto& g : gamepads_) {
        g.pressed.reset();
        g.released.reset();
    }
}

Key key_from_sdl(i32 sdl_keycode) {
    using K = Key;
    switch (sdl_keycode) {
        case SDLK_A: return K::A; case SDLK_B: return K::B; case SDLK_C: return K::C;
        case SDLK_D: return K::D; case SDLK_E: return K::E; case SDLK_F: return K::F;
        case SDLK_G: return K::G; case SDLK_H: return K::H; case SDLK_I: return K::I;
        case SDLK_J: return K::J; case SDLK_K: return K::K; case SDLK_L: return K::L;
        case SDLK_M: return K::M; case SDLK_N: return K::N; case SDLK_O: return K::O;
        case SDLK_P: return K::P; case SDLK_Q: return K::Q; case SDLK_R: return K::R;
        case SDLK_S: return K::S; case SDLK_T: return K::T; case SDLK_U: return K::U;
        case SDLK_V: return K::V; case SDLK_W: return K::W; case SDLK_X: return K::X;
        case SDLK_Y: return K::Y; case SDLK_Z: return K::Z;
        case SDLK_0: return K::Num0; case SDLK_1: return K::Num1; case SDLK_2: return K::Num2;
        case SDLK_3: return K::Num3; case SDLK_4: return K::Num4; case SDLK_5: return K::Num5;
        case SDLK_6: return K::Num6; case SDLK_7: return K::Num7; case SDLK_8: return K::Num8;
        case SDLK_9: return K::Num9;
        case SDLK_F1: return K::F1; case SDLK_F2: return K::F2; case SDLK_F3: return K::F3;
        case SDLK_F4: return K::F4; case SDLK_F5: return K::F5; case SDLK_F6: return K::F6;
        case SDLK_F7: return K::F7; case SDLK_F8: return K::F8; case SDLK_F9: return K::F9;
        case SDLK_F10: return K::F10; case SDLK_F11: return K::F11; case SDLK_F12: return K::F12;
        case SDLK_ESCAPE: return K::Escape; case SDLK_TAB: return K::Tab; case SDLK_SPACE: return K::Space;
        case SDLK_RETURN: return K::Enter; case SDLK_BACKSPACE: return K::Backspace;
        case SDLK_INSERT: return K::Insert; case SDLK_DELETE: return K::Delete;
        case SDLK_HOME: return K::Home; case SDLK_END: return K::End;
        case SDLK_PAGEUP: return K::PageUp; case SDLK_PAGEDOWN: return K::PageDown;
        case SDLK_LEFT: return K::Left; case SDLK_RIGHT: return K::Right;
        case SDLK_UP: return K::Up; case SDLK_DOWN: return K::Down;
        case SDLK_LSHIFT: return K::LeftShift; case SDLK_RSHIFT: return K::RightShift;
        case SDLK_LCTRL:  return K::LeftCtrl;  case SDLK_RCTRL:  return K::RightCtrl;
        case SDLK_LALT:   return K::LeftAlt;   case SDLK_RALT:   return K::RightAlt;
        case SDLK_LGUI:   return K::LeftSuper; case SDLK_RGUI:   return K::RightSuper;
        case SDLK_COMMA:  return K::Comma; case SDLK_PERIOD:    return K::Period;
        case SDLK_SLASH:  return K::Slash; case SDLK_SEMICOLON: return K::Semicolon;
        case SDLK_APOSTROPHE: return K::Apostrophe; case SDLK_GRAVE: return K::Backquote;
        case SDLK_LEFTBRACKET: return K::LeftBracket; case SDLK_RIGHTBRACKET: return K::RightBracket;
        case SDLK_BACKSLASH: return K::Backslash; case SDLK_MINUS: return K::Minus;
        case SDLK_EQUALS: return K::Equal;
        default: return K::None;
    }
}

static MouseButton mb_from_sdl(u8 b) {
    switch (b) {
        case SDL_BUTTON_LEFT:   return MouseButton::Left;
        case SDL_BUTTON_RIGHT:  return MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return MouseButton::Middle;
        case SDL_BUTTON_X1:     return MouseButton::X1;
        case SDL_BUTTON_X2:     return MouseButton::X2;
        default: return MouseButton::Left;
    }
}

static GamepadButton gb_from_sdl(u8 b) {
    switch (b) {
        case SDL_GAMEPAD_BUTTON_SOUTH:        return GamepadButton::A;
        case SDL_GAMEPAD_BUTTON_EAST:         return GamepadButton::B;
        case SDL_GAMEPAD_BUTTON_WEST:         return GamepadButton::X;
        case SDL_GAMEPAD_BUTTON_NORTH:        return GamepadButton::Y;
        case SDL_GAMEPAD_BUTTON_BACK:         return GamepadButton::Back;
        case SDL_GAMEPAD_BUTTON_GUIDE:        return GamepadButton::Guide;
        case SDL_GAMEPAD_BUTTON_START:        return GamepadButton::Start;
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:   return GamepadButton::LeftStick;
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:  return GamepadButton::RightStick;
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return GamepadButton::LeftShoulder;
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return GamepadButton::RightShoulder;
        case SDL_GAMEPAD_BUTTON_DPAD_UP:    return GamepadButton::DPadUp;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:  return GamepadButton::DPadDown;
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:  return GamepadButton::DPadLeft;
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return GamepadButton::DPadRight;
        default: return GamepadButton::Count;
    }
}

static GamepadAxis ga_from_sdl(u8 a) {
    switch (a) {
        case SDL_GAMEPAD_AXIS_LEFTX:        return GamepadAxis::LeftX;
        case SDL_GAMEPAD_AXIS_LEFTY:        return GamepadAxis::LeftY;
        case SDL_GAMEPAD_AXIS_RIGHTX:       return GamepadAxis::RightX;
        case SDL_GAMEPAD_AXIS_RIGHTY:       return GamepadAxis::RightY;
        case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return GamepadAxis::LeftTrigger;
        case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return GamepadAxis::RightTrigger;
        default: return GamepadAxis::Count;
    }
}

void Input::on_sdl_event(void* sdl_ev) {
    auto* ev = static_cast<SDL_Event*>(sdl_ev);
    switch (ev->type) {
        case SDL_EVENT_KEY_DOWN: {
            if (!ev->key.repeat) {
                Key k = key_from_sdl(ev->key.key);
                if (k != Key::None) {
                    auto i = static_cast<usize>(k);
                    if (!key_down_[i]) key_pressed_[i] = true;
                    key_down_[i] = true;
                }
            }
        } break;
        case SDL_EVENT_KEY_UP: {
            Key k = key_from_sdl(ev->key.key);
            if (k != Key::None) {
                auto i = static_cast<usize>(k);
                if (key_down_[i]) key_released_[i] = true;
                key_down_[i] = false;
            }
        } break;
        case SDL_EVENT_TEXT_INPUT: {
            if (ev->text.text) text_input_ += ev->text.text;
        } break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            MouseButton b = mb_from_sdl(ev->button.button);
            auto i = static_cast<usize>(b);
            if (!mouse_down_[i]) mouse_pressed_[i] = true;
            mouse_down_[i] = true;
        } break;
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            MouseButton b = mb_from_sdl(ev->button.button);
            auto i = static_cast<usize>(b);
            if (mouse_down_[i]) mouse_released_[i] = true;
            mouse_down_[i] = false;
        } break;
        case SDL_EVENT_MOUSE_MOTION:
            mouse_pos_   = math::vec2(ev->motion.x, ev->motion.y);
            mouse_delta_ += math::vec2(ev->motion.xrel, ev->motion.yrel);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            wheel_ += ev->wheel.y;
            break;
        case SDL_EVENT_GAMEPAD_ADDED:
            on_gamepad_added_(ev->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_REMOVED:
            on_gamepad_removed_(ev->gdevice.which);
            break;
        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        case SDL_EVENT_GAMEPAD_BUTTON_UP: {
            for (auto& g : gamepads_) {
                if (!g.connected) continue;
                if (SDL_GetGamepadID((SDL_Gamepad*)g.sdl_handle) != ev->gbutton.which) continue;
                GamepadButton gb = gb_from_sdl(ev->gbutton.button);
                if (gb == GamepadButton::Count) break;
                auto i = static_cast<usize>(gb);
                bool down = (ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
                if (down && !g.down[i]) g.pressed[i]  = true;
                if (!down && g.down[i]) g.released[i] = true;
                g.down[i] = down;
                break;
            }
        } break;
        case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
            for (auto& g : gamepads_) {
                if (!g.connected) continue;
                if (SDL_GetGamepadID((SDL_Gamepad*)g.sdl_handle) != ev->gaxis.which) continue;
                GamepadAxis ga = ga_from_sdl(ev->gaxis.axis);
                if (ga == GamepadAxis::Count) break;
                f32 v = static_cast<f32>(ev->gaxis.value) / 32767.0f;
                // Apply small dead-zone to sticks; triggers stay 0..1.
                if (ga == GamepadAxis::LeftTrigger || ga == GamepadAxis::RightTrigger) {
                    v = math::saturate(v);
                } else {
                    constexpr f32 dz = 0.18f;
                    if (std::abs(v) < dz) v = 0.0f;
                    else v = (v - (v > 0 ? dz : -dz)) / (1.0f - dz);
                }
                g.axis[static_cast<usize>(ga)] = v;
                break;
            }
        } break;
        default: break;
    }
}

void Input::on_gamepad_added_(i32 sdl_id) {
    SDL_Gamepad* gp = SDL_OpenGamepad(sdl_id);
    if (!gp) return;
    for (u32 i = 0; i < kMaxGamepads; ++i) {
        if (!gamepads_[i].connected) {
            gamepads_[i].connected = true;
            gamepads_[i].sdl_handle = gp;
            gamepads_[i].name = SDL_GetGamepadName(gp) ? SDL_GetGamepadName(gp) : "Gamepad";
            CN_INFO("input", "gamepad {} connected: {}", i, gamepads_[i].name);
            return;
        }
    }
    SDL_CloseGamepad(gp);
}

void Input::on_gamepad_removed_(i32 sdl_id) {
    for (auto& g : gamepads_) {
        if (g.connected && SDL_GetGamepadID((SDL_Gamepad*)g.sdl_handle) == sdl_id) {
            SDL_CloseGamepad((SDL_Gamepad*)g.sdl_handle);
            CN_INFO("input", "gamepad disconnected: {}", g.name);
            g = GamepadState{};
            return;
        }
    }
}

void Input::rumble(u32 idx, f32 lo, f32 hi, u32 ms) {
    if (idx >= kMaxGamepads || !gamepads_[idx].connected) return;
    auto* gp = (SDL_Gamepad*)gamepads_[idx].sdl_handle;
    SDL_RumbleGamepad(gp, (u16)(lo * 65535.0f), (u16)(hi * 65535.0f), ms);
}

bool Input::key_down    (Key k) const { return key_down_[static_cast<usize>(k)]; }
bool Input::key_pressed (Key k) const { return key_pressed_[static_cast<usize>(k)]; }
bool Input::key_released(Key k) const { return key_released_[static_cast<usize>(k)]; }

bool Input::mouse_down    (MouseButton b) const { return mouse_down_[static_cast<usize>(b)]; }
bool Input::mouse_pressed (MouseButton b) const { return mouse_pressed_[static_cast<usize>(b)]; }
bool Input::mouse_released(MouseButton b) const { return mouse_released_[static_cast<usize>(b)]; }

} // namespace cn::platform
