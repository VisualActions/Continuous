// Continuous editor entry.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "EditorApp.h"

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    cnedit::EditorApp app;
    return app.run(__argc, __argv);
}
