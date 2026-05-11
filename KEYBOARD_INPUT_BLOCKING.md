# Blocking All Keyboard Input to Other SKSE Mods When Your UI Is Open

## Problem

Skyrim's SKSE plugin ecosystem uses `BSInputDeviceManager` to poll the keyboard each frame and dispatch `ButtonEvent`s to every registered `BSTEventSink<InputEvent*>`. If your mod opens a text-input overlay (e.g. a browser-based UI), any keys the user types will also fire hotkeys registered by other mods. There is no high-level "consume input" API — you must block at the source.

## Root Cause / Why It Works

Every game frame, `Main::Update()` calls `BSInputDeviceManager::PollInputDevices()`, which calls `BSWin32KeyboardDevice::Poll(float)` (vtable slot **2**). That function reads raw scan codes out of the DirectInput 8 device buffer and converts them into `ButtonEvent`s. If the buffer is empty when `Poll` runs, no events are generated — for anyone.

The technique is to **swap vtable slot 2** to point at your own function. Before forwarding to the real `Poll`, if your UI is active, you drain and discard the DirectInput buffer entirely. The original `Poll` then reads nothing and emits nothing.

---

## Prerequisites

- CommonLibSSE-NG (headers: `RE/B/BSWin32KeyboardDevice.h`, `RE/B/BSInputDeviceManager.h`, `REX/W32/DINPUT.h`)
- `REL::safe_write` from CommonLibSSE-NG (handles `VirtualProtect` for you)
- The vtable ID `VTABLE_BSWin32KeyboardDevice` from `RE/Offsets_VTABLE.h`
- No MinHook needed — this is a raw vtable patch, not an IAT/inline hook

---

## Step 1 — State Flag

You need a boolean that is `true` while your UI is consuming input and `false` otherwise:

```cpp
// Can be a member of your manager class or a plain static
static bool g_inputIgnored = false;

bool IsInputIgnored() { return g_inputIgnored; }
void SetInputIgnored(bool ignore) { g_inputIgnored = ignore; }
```

Call `SetInputIgnored(true)` when your overlay opens, `SetInputIgnored(false)` when it closes.

---

## Step 2 — The Flush Helper

This drains the DirectInput 8 device buffer so there is nothing left for `Poll` to read:

```cpp
#include "RE/B/BSWin32KeyboardDevice.h"
#include "REX/W32/DINPUT.h"

static void FlushKeyboard(RE::BSWin32KeyboardDevice* device) {
    // NOTE: CommonLib declares the field as IDirectInput8A* but Skyrim actually stores
    // IDirectInputDevice8A* (the per-device interface, not the top-level factory).
    // A reinterpret_cast is required.
    auto* inputDevice = reinterpret_cast<REX::W32::IDirectInputDevice8A*>(
        device->GetRuntimeData().dInputDevice);

    if (!inputDevice) return;

    REX::W32::DIDEVICEOBJECTDATA flushData[10];
    uint32_t count = 10;

    // Keep reading in chunks of 10 until the buffer is empty.
    // GetDeviceData with flags = 0 (last arg) is a consuming read.
    while (count > 0) {
        count = 10;
        if (REX::W32::SUCCESS(inputDevice->Acquire())) {
            inputDevice->Poll();   // IDirectInputDevice8A::Poll — advances hardware state
            if (inputDevice->GetDeviceData(
                    sizeof(REX::W32::DIDEVICEOBJECTDATA),
                    flushData, &count, 0) != 0) {
                break;  // error — stop trying
            }
        } else {
            break;  // could not acquire — stop
        }
    }

    // Also zero out the cached key-state arrays inside BSWin32KeyboardDevice
    // (prevState/curState). This prevents the game from seeing stale "held" states.
    device->ClearInputState();
}
```

Key notes:
- `inputDevice->Acquire()` returns `HRESULT`; `REX::W32::SUCCESS(hr)` is `hr >= 0`.
- `GetDeviceData` with `flags = 0` is a **consuming** read — it removes the events from the buffer.
- `ClearInputState()` is vtable slot 08 on `BSWin32KeyboardDevice`. It does `memset(curState, 0, 0x200)`. Without this, `IsPressed()` queries may still see stale state.

---

## Step 3 — The Vtable Hook Function

```cpp
// Store the original so we can forward the call after optionally flushing.
static void (*g_originalPoll)(RE::BSWin32KeyboardDevice* device, float timeDelta) = nullptr;

void KeyboardPollHook(RE::BSWin32KeyboardDevice* device, float timeDelta) {
    if (IsInputIgnored()) {
        if (device) {
            FlushKeyboard(device);
        }
        // Still call original — it will read an empty buffer and emit nothing.
        // Alternatively you can early-return here if you want to suppress all
        // BSInputDevice internal state updates too, but calling through is safer.
    }
    if (g_originalPoll) {
        g_originalPoll(device, timeDelta);
    }
}
```

Whether to call the original after flushing is a design choice:
- **Call original (recommended)**: the game's internal book-keeping (`prevState`/`curState` inside `BSWin32KeyboardDevice`) stays consistent; pressing keys just doesn't generate events.
- **Skip original**: marginally harder to cause a stuck-key bug, but the internal state can desync from reality. Generally not necessary because `FlushKeyboard` already calls `ClearInputState`.

---

## Step 4 — Install the Hook

Call this **once**, after `BSInputDeviceManager` is available (typically during or after `SKSEMessagingInterface::kMessage_PostPostLoad` or `kMessage_InputLoaded`):

```cpp
#include "RE/B/BSInputDeviceManager.h"
#include "RE/Offsets_VTABLE.h"  // for VTABLE_BSWin32KeyboardDevice
#include "REL/Relocation.h"     // for REL::safe_write

void InstallKeyboardInputBlocker() {
    auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputDeviceManager) return;

    auto* keyboard = static_cast<RE::BSInputDevice*>(inputDeviceManager->GetKeyboard());
    if (!keyboard) return;

    // Read the vtable pointer from the object.
    // keyboard_vtable[N] is a raw function pointer for virtual slot N.
    auto* keyboard_vtable = *reinterpret_cast<uintptr_t**>(keyboard);

    // SAFETY CHECK: verify this is actually a BSWin32KeyboardDevice, not some
    // other subclass (e.g. VR has BSTrackedControllerDevice).
    if (keyboard_vtable != reinterpret_cast<uintptr_t*>(
            RE::VTABLE_BSWin32KeyboardDevice[0].address())) {
        // Not the keyboard we expect — do not patch.
        return;
    }

    // Vtable slot 2 = Poll(float). Save original, write our hook.
    g_originalPoll = reinterpret_cast<void(*)(RE::BSWin32KeyboardDevice*, float)>(
        keyboard_vtable[2]);

    uint64_t hookAddr = reinterpret_cast<uint64_t>(&KeyboardPollHook);

    // REL::safe_write calls VirtualProtect, writes the pointer, then restores protection.
    REL::safe_write(
        reinterpret_cast<uintptr_t>(keyboard_vtable + 2),
        &hookAddr,
        sizeof(uintptr_t));
}
```

### Vtable Slot Reference

From `BSIInputDevice` and `BSWin32KeyboardDevice` headers:

| Slot | Method |
|------|--------|
| 0 | `~destructor` |
| 1 | `Initialize()` |
| **2** | **`Poll(float)`** ← patch this one |
| 3 | `Shutdown()` |
| 4 | `GetButtonNameFromID` |
| 5 | `GetMappingKey` |
| 6 | `GetKeyCodeFromID` |
| 7 | `IsEnabled` |
| 8 | `ClearInputState()` |

---

## Step 5 — Toggle on Open/Close

```cpp
void OnOverlayOpen() {
    SetInputIgnored(true);
}

void OnOverlayClose() {
    SetInputIgnored(false);
}
```

---

## What This Blocks (and What It Does Not)

| Blocked | Not Blocked |
|---------|-------------|
| All `BSTEventSink<InputEvent*>` handlers in all mods | `GetAsyncKeyState()` polling — reads OS state, bypasses DirectInput entirely |
| Skyrim's own control map (movement, menu open, etc.) | Any mod that calls `GetAsyncKeyState` or `GetKeyState` directly |
| SKSE's `InputEventHandler` | WndProc-level key messages (`WM_KEYDOWN`) |

### The `GetAsyncKeyState` Gap

`GetAsyncKeyState()` reads raw hardware state from the OS kernel and completely bypasses DirectInput. This vtable hook cannot stop it. If your own mod also uses `GetAsyncKeyState`-based hotkey polling, you must add a separate guard: check whether your overlay has focus (e.g. via a UI focus API) and return early from the hotkey handler.

Example pattern:

```cpp
void OnHotkeyPressed() {
    if (MyUI::GetInstance().IsOpen())
        return;  // UI is consuming keyboard — ignore
    // ... handle hotkey
}
```

---

## Cleanup / Uninstall

If you need to remove the hook (e.g. on plugin shutdown):

```cpp
void UninstallKeyboardInputBlocker() {
    auto* inputDeviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (!inputDeviceManager || !g_originalPoll) return;

    auto* keyboard = static_cast<RE::BSInputDevice*>(inputDeviceManager->GetKeyboard());
    if (!keyboard) return;

    auto* keyboard_vtable = *reinterpret_cast<uintptr_t**>(keyboard);
    uint64_t originalAddr = reinterpret_cast<uint64_t>(g_originalPoll);
    REL::safe_write(
        reinterpret_cast<uintptr_t>(keyboard_vtable + 2),
        &originalAddr,
        sizeof(uintptr_t));
    g_originalPoll = nullptr;
}
```

---

## Complete Minimal Example

```cpp
// KeyboardBlocker.h
#pragma once
void InstallKeyboardInputBlocker();
void UninstallKeyboardInputBlocker();
void SetKeyboardInputBlocked(bool blocked);
```

```cpp
// KeyboardBlocker.cpp
#include "RE/B/BSWin32KeyboardDevice.h"
#include "RE/B/BSInputDeviceManager.h"
#include "RE/Offsets_VTABLE.h"
#include "REL/Relocation.h"
#include "REX/W32/DINPUT.h"
#include <atomic>

static std::atomic<bool> g_blocked{false};
static void (*g_originalPoll)(RE::BSWin32KeyboardDevice*, float) = nullptr;

void SetKeyboardInputBlocked(bool blocked) { g_blocked.store(blocked); }

static void FlushKeyboard(RE::BSWin32KeyboardDevice* device) {
    auto* di = reinterpret_cast<REX::W32::IDirectInputDevice8A*>(
        device->GetRuntimeData().dInputDevice);
    if (!di) return;
    REX::W32::DIDEVICEOBJECTDATA buf[10];
    uint32_t count = 10;
    while (count > 0) {
        count = 10;
        if (REX::W32::SUCCESS(di->Acquire())) {
            di->Poll();
            if (di->GetDeviceData(sizeof(buf[0]), buf, &count, 0) != 0) break;
        } else break;
    }
    device->ClearInputState();
}

static void PollHook(RE::BSWin32KeyboardDevice* device, float dt) {
    if (g_blocked.load() && device) FlushKeyboard(device);
    if (g_originalPoll) g_originalPoll(device, dt);
}

void InstallKeyboardInputBlocker() {
    auto* mgr = RE::BSInputDeviceManager::GetSingleton();
    if (!mgr) return;
    auto* kb = static_cast<RE::BSInputDevice*>(mgr->GetKeyboard());
    if (!kb) return;
    auto* vtbl = *reinterpret_cast<uintptr_t**>(kb);
    if (vtbl != reinterpret_cast<uintptr_t*>(RE::VTABLE_BSWin32KeyboardDevice[0].address()))
        return;
    g_originalPoll = reinterpret_cast<void(*)(RE::BSWin32KeyboardDevice*, float)>(vtbl[2]);
    uint64_t h = reinterpret_cast<uint64_t>(&PollHook);
    REL::safe_write(reinterpret_cast<uintptr_t>(vtbl + 2), &h, sizeof(h));
}

void UninstallKeyboardInputBlocker() {
    auto* mgr = RE::BSInputDeviceManager::GetSingleton();
    if (!mgr || !g_originalPoll) return;
    auto* kb = static_cast<RE::BSInputDevice*>(mgr->GetKeyboard());
    if (!kb) return;
    auto* vtbl = *reinterpret_cast<uintptr_t**>(kb);
    uint64_t orig = reinterpret_cast<uint64_t>(g_originalPoll);
    REL::safe_write(reinterpret_cast<uintptr_t>(vtbl + 2), &orig, sizeof(orig));
    g_originalPoll = nullptr;
}
```
