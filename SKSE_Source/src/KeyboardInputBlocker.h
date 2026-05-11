#pragma once
#include "PCH.h"
#include <vector>
#include <functional>

// Blocks all DirectInput keyboard events from reaching other mods/the game
// while OStim Prism UI has focus, by patching BSWin32KeyboardDevice vtable slot 2 (Process).
namespace KeyboardInputBlocker {
    // Install the vtable hook. Call once at kInputLoaded.
    void Install();

    // Toggle input blocking.
    void SetBlocking(bool block);

    // Keys to monitor. When blocking, transitions on these keys call the key handler instead
    // of going through DInput/BSInputDeviceManager.
    void SetAllowedKeys(std::vector<uint32_t> keys);

    // Called for each allowed key state transition: (keyCode, isDown).
    // Runs on the main game thread inside BSInputDeviceManager::PollInputDevices.
    using KeyHandler = std::function<void(uint32_t key, bool isDown)>;
    void SetKeyHandler(KeyHandler handler);
}
