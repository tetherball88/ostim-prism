#include "KeyboardInputBlocker.h"
#include "RE/B/BSWin32KeyboardDevice.h"
#include "RE/B/BSInputDeviceManager.h"
#include "RE/Offsets_VTABLE.h"
#include "REX/W32/DINPUT.h"
#include "REL/Relocation.h"

namespace KeyboardInputBlocker {
    namespace {
        static bool g_blocking = false;
        static std::vector<uint32_t> g_allowedKeys;
        static uint8_t g_prevAllowedState[256] = {};  // our own hardware state tracker
        static KeyHandler g_keyHandler;

        // Original BSWin32KeyboardDevice::Process(float) — saved before patching.
        static void (*g_originalProcess)(RE::BSWin32KeyboardDevice* device, float timeDelta) = nullptr;

        // Fully drains the DInput buffer and zeroes device state.
        static void FlushKeyboard(RE::BSWin32KeyboardDevice* device) {
            auto* inputDevice = reinterpret_cast<REX::W32::IDirectInputDevice8A*>(device->dInputDevice);
            if (!inputDevice) return;

            if (!REX::W32::SUCCESS(inputDevice->Acquire())) return;
            inputDevice->Poll();

            REX::W32::DIDEVICEOBJECTDATA buf[32];
            uint32_t count = 32;
            while (count > 0) {
                count = 32;
                if (inputDevice->GetDeviceData(sizeof(REX::W32::DIDEVICEOBJECTDATA), buf, &count, 0) != 0) break;
            }

            device->Reset();
        }

        static void KeyboardProcessHook(RE::BSWin32KeyboardDevice* device, float timeDelta) {
            if (g_blocking && device) {
                auto* inputDevice = reinterpret_cast<REX::W32::IDirectInputDevice8A*>(device->dInputDevice);

                // Snapshot current hardware state before draining.
                uint8_t rawState[256] = {};
                if (inputDevice && REX::W32::SUCCESS(inputDevice->Acquire())) {
                    inputDevice->Poll();
                    inputDevice->GetDeviceState(sizeof(rawState), rawState);
                }

                // Fire callbacks for allowed key transitions.
                if (g_keyHandler) {
                    for (uint32_t key : g_allowedKeys) {
                        if (key >= 256) continue;
                        bool wasDown = g_prevAllowedState[key] & 0x80;
                        bool isDown  = rawState[key] & 0x80;
                        if (wasDown != isDown) {
                            SKSE::log::info("KeyboardInputBlocker: allowed key {} {} — calling handler", key, isDown ? "down" : "up");
                            g_keyHandler(key, isDown);
                        }
                        g_prevAllowedState[key] = rawState[key];
                    }
                }

                FlushKeyboard(device);
            }
            if (g_originalProcess) {
                g_originalProcess(device, timeDelta);
            }
        }
    }

    void Install() {
        auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
        if (!inputDeviceManager) {
            SKSE::log::error("KeyboardInputBlocker: BSInputDeviceManager not available");
            return;
        }

        auto* keyboard = inputDeviceManager->GetKeyboard();
        if (!keyboard) {
            SKSE::log::error("KeyboardInputBlocker: keyboard device not available");
            return;
        }

        // Read the vtable pointer from the object instance.
        auto* vtable = *reinterpret_cast<uintptr_t**>(keyboard);

        // Safety check: ensure this is actually a BSWin32KeyboardDevice, not a VR controller
        // or other subclass that happens to sit in the keyboard slot.
        const uintptr_t expectedVtable = RE::VTABLE_BSWin32KeyboardDevice[0].address();
        if (vtable != reinterpret_cast<uintptr_t*>(expectedVtable)) {
            SKSE::log::error("KeyboardInputBlocker: unexpected keyboard vtable — not patching");
            return;
        }

        // Vtable slot 2 = Process(float). Save the original, then write our hook.
        g_originalProcess = reinterpret_cast<void (*)(RE::BSWin32KeyboardDevice*, float)>(vtable[2]);

        const auto hookAddr = reinterpret_cast<uint64_t>(&KeyboardProcessHook);
        REL::safe_write(reinterpret_cast<uintptr_t>(vtable + 2), hookAddr);

        SKSE::log::info("KeyboardInputBlocker: vtable hook installed at BSWin32KeyboardDevice::Process (slot 2)");
    }

    void SetBlocking(bool block) {
        g_blocking = block;
        SKSE::log::info("KeyboardInputBlocker: blocking={}", block);
    }

    void SetAllowedKeys(std::vector<uint32_t> keys) {
        std::string keyList;
        for (uint32_t k : keys) keyList += std::to_string(k) + " ";
        SKSE::log::info("KeyboardInputBlocker: SetAllowedKeys [{}]", keyList);
        // Reset tracked state so transitions are detected fresh on next block.
        std::memset(g_prevAllowedState, 0, sizeof(g_prevAllowedState));
        g_allowedKeys = std::move(keys);
    }

    void SetKeyHandler(KeyHandler handler) {
        g_keyHandler = std::move(handler);
    }
}
