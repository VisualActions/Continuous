// Continuous Engine - input state (keyboard, mouse, gamepad).
//
// Simple polling API: the engine pumps SDL events each frame, updates the
// global Input state, and gameplay reads from it via Input::get(). Edge events
// are exposed as pressed()/released(). Gamepads are auto-detected and assigned
// stable indices [0..kMaxGamepads).
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/math/Math.h"

#include <array>
#include <bitset>
#include <string>

namespace cn::platform {

enum class Key : u16 {
    None = 0,
    A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,
    Num0,Num1,Num2,Num3,Num4,Num5,Num6,Num7,Num8,Num9,
    F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,F11,F12,
    Escape, Tab, Space, Enter, Backspace, Insert, Delete, Home, End, PageUp, PageDown,
    Left, Right, Up, Down,
    LeftShift, RightShift, LeftCtrl, RightCtrl, LeftAlt, RightAlt, LeftSuper, RightSuper,
    Comma, Period, Slash, Semicolon, Apostrophe, Backquote,
    LeftBracket, RightBracket, Backslash, Minus, Equal,
    Count
};

enum class MouseButton : u8 {
    Left = 0, Right, Middle, X1, X2, Count
};

enum class GamepadButton : u8 {
    A = 0, B, X, Y,
    Back, Guide, Start,
    LeftStick, RightStick,
    LeftShoulder, RightShoulder,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    Count
};

enum class GamepadAxis : u8 {
    LeftX = 0, LeftY,
    RightX, RightY,
    LeftTrigger, RightTrigger,
    Count
};

constexpr u32 kMaxGamepads = 4;

struct GamepadState {
    bool                                          connected{false};
    void*                                         sdl_handle{nullptr};
    std::string                                   name;
    std::bitset<static_cast<usize>(GamepadButton::Count)> down;
    std::bitset<static_cast<usize>(GamepadButton::Count)> pressed;
    std::bitset<static_cast<usize>(GamepadButton::Count)> released;
    std::array<f32, static_cast<usize>(GamepadAxis::Count)> axis{};
};

class CN_API Input {
public:
    static Input& get();

    // Called once per frame at the start, before pumping events, to clear the
    // edge bits.
    void begin_frame();

    // Called from the SDL event loop with raw SDL_Event*.
    void on_sdl_event(void* sdl_event);

    // Keyboard.
    bool key_down    (Key k) const;
    bool key_pressed (Key k) const;  // edge: this frame
    bool key_released(Key k) const;

    // Mouse.
    bool mouse_down    (MouseButton b) const;
    bool mouse_pressed (MouseButton b) const;
    bool mouse_released(MouseButton b) const;

    math::vec2 mouse_position() const { return mouse_pos_; }
    math::vec2 mouse_delta()    const { return mouse_delta_; }
    f32        wheel()          const { return wheel_; }

    // Gamepads.
    const GamepadState& gamepad(u32 idx) const { return gamepads_[idx < kMaxGamepads ? idx : 0]; }
    bool                gamepad_connected(u32 idx) const { return idx < kMaxGamepads && gamepads_[idx].connected; }

    void rumble(u32 idx, f32 lo, f32 hi, u32 ms);

    // Text input (collected from SDL_TEXTINPUT).
    const std::string& text_input() const { return text_input_; }

private:
    Input() = default;
    void on_gamepad_added_(i32 sdl_id);
    void on_gamepad_removed_(i32 sdl_id);

    std::bitset<static_cast<usize>(Key::Count)> key_down_, key_pressed_, key_released_;
    std::bitset<static_cast<usize>(MouseButton::Count)> mouse_down_, mouse_pressed_, mouse_released_;

    math::vec2 mouse_pos_  {0, 0};
    math::vec2 mouse_delta_{0, 0};
    f32        wheel_      = 0.0f;

    std::array<GamepadState, kMaxGamepads> gamepads_;
    std::string text_input_;
};

CN_API Key key_from_sdl(i32 sdl_keycode);

} // namespace cn::platform
