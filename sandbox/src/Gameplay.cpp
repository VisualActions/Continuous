// Gameplay.cpp - the gameplay DLL entry point.
//
// The engine looks up "cn_gameplay_register" via GetProcAddress after every
// LoadLibrary. Calling this function references the registry and pulls in
// the auto-registers that live in each behavior TU.

#include "continuous/HotReload.h"

extern "C" CN_GAMEPLAY_API void cn_gameplay_register() {
    // The CN_GAMEPLAY_REGISTER macros in PlayerController/Spinner/AudioBlip/
    // NetDemoController register themselves at static init time. Calling this
    // function is enough to keep those TU's reachable.
    auto& r = cn::gameplay::Registry::get();
    (void)r;
}
