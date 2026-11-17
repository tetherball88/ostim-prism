#include "PCH.h"
#include "PrismaUIManager.h"
#include "PortraitManager.h"
#include "Settings.h"
#include "REX/W32/XINPUT.h"
#include <thread>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <Windows.h>
#include <algorithm>
#include <filesystem>

namespace {
    int64_t SteadyNowMs() {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    }

    bool IsGameWindowForeground() {
        HWND fg = GetForegroundWindow();
        if (!fg) {
            return false;
        }
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        return pid == GetCurrentProcessId();
    }

    int DxScanCodeToVirtualKey(uint32_t code) {
        if (code >= 257 && code <= 260) {
            switch (code) {
                case 257: return VK_RBUTTON;
                case 258: return VK_MBUTTON;
                case 259: return VK_XBUTTON1;
                case 260: return VK_XBUTTON2;
                default:  return 0;
            }
        }
        if (code == 0 || code >= 256) {
            return 0;
        }
        switch (code) {
            case 42:  return VK_LSHIFT;
            case 54:  return VK_RSHIFT;
            case 29:  return VK_LCONTROL;
            case 157: return VK_RCONTROL;
            case 56:  return VK_LMENU;
            case 184: return VK_RMENU;
            case 156: return VK_RETURN;
            case 181: return VK_DIVIDE;
            case 197: return VK_PAUSE;
            case 199: return VK_HOME;
            case 200: return VK_UP;
            case 201: return VK_PRIOR;
            case 203: return VK_LEFT;
            case 205: return VK_RIGHT;
            case 207: return VK_END;
            case 208: return VK_DOWN;
            case 209: return VK_NEXT;
            case 210: return VK_INSERT;
            case 211: return VK_DELETE;
            default: {
                const UINT mapped = (code < 128)
                    ? MapVirtualKeyW(code, MAPVK_VSC_TO_VK_EX)
                    : MapVirtualKeyW(0xE000u | (code & 0x7Fu), MAPVK_VSC_TO_VK_EX);
                return static_cast<int>(mapped);
            }
        }
    }

    bool IsVkDown(int vk) {
        return vk > 0 && (GetAsyncKeyState(vk) & 0x8000) != 0;
    }

    bool IsScanCodePhysicallyDown(uint32_t code) {
        if (code == 0 || code >= 266) {
            return false;
        }
        if (code == 256) {
            return IsVkDown(VK_LBUTTON);
        }
        if (code >= 257 && code <= 260) {
            return IsVkDown(DxScanCodeToVirtualKey(code));
        }
        if (code >= 256) {
            return false;
        }
        switch (code) {
            case 71: return IsVkDown(VK_NUMPAD7) || IsVkDown(VK_HOME);
            case 72: return IsVkDown(VK_NUMPAD8) || IsVkDown(VK_UP);
            case 73: return IsVkDown(VK_NUMPAD9) || IsVkDown(VK_PRIOR);
            case 74: return IsVkDown(VK_SUBTRACT);
            case 75: return IsVkDown(VK_NUMPAD4) || IsVkDown(VK_LEFT);
            case 76: return IsVkDown(VK_NUMPAD5) || IsVkDown(VK_CLEAR);
            case 77: return IsVkDown(VK_NUMPAD6) || IsVkDown(VK_RIGHT);
            case 78: return IsVkDown(VK_ADD);
            case 79: return IsVkDown(VK_NUMPAD1) || IsVkDown(VK_END);
            case 80: return IsVkDown(VK_NUMPAD2) || IsVkDown(VK_DOWN);
            case 81: return IsVkDown(VK_NUMPAD3) || IsVkDown(VK_NEXT);
            case 82: return IsVkDown(VK_NUMPAD0) || IsVkDown(VK_INSERT);
            case 83: return IsVkDown(VK_DECIMAL) || IsVkDown(VK_DELETE);
            default: return IsVkDown(DxScanCodeToVirtualKey(code));
        }
    }

    bool QueryXInputState(DWORD slot, REX::W32::XINPUT_STATE& outState) {
        using XInputGetStateFn = DWORD(WINAPI*)(DWORD, REX::W32::XINPUT_STATE*);
        static XInputGetStateFn s_fn = []() -> XInputGetStateFn {
            static constexpr const wchar_t* kDlls[] = {
                L"xinput1_4.dll", L"xinput1_3.dll", L"xinput9_1_0.dll"
            };
            for (const wchar_t* dll : kDlls) {
                HMODULE mod = GetModuleHandleW(dll);
                if (!mod) {
                    mod = LoadLibraryW(dll);
                }
                if (mod) {
                    auto fn = reinterpret_cast<XInputGetStateFn>(GetProcAddress(mod, "XInputGetState"));
                    if (fn) {
                        return fn;
                    }
                }
            }
            return nullptr;
        }();
        if (!s_fn) {
            return false;
        }
        return s_fn(slot, &outState) == ERROR_SUCCESS;
    }

    uint32_t BuildXInputMask(const REX::W32::XINPUT_GAMEPAD& pad) {
        static constexpr uint16_t kDigitalMasks[14] = {
            0x0001, 0x0002, 0x0004, 0x0008,
            0x0010, 0x0020, 0x0040, 0x0080,
            0x0100, 0x0200,
            0x1000, 0x2000, 0x4000, 0x8000
        };
        uint32_t mask = 0;
        for (uint32_t i = 0; i < 14; ++i) {
            if ((pad.buttons & kDigitalMasks[i]) != 0) {
                mask |= (1u << i);
            }
        }
        if (pad.leftTrigger > REX::W32::XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
            mask |= (1u << 14);
        }
        if (pad.rightTrigger > REX::W32::XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
            mask |= (1u << 15);
        }
        return mask;
    }

    bool TryConsumeDebounce(std::atomic<int64_t>& lastMs, int64_t windowMs = 200) {
        const int64_t now = SteadyNowMs();
        const int64_t prev = lastMs.load(std::memory_order_relaxed);
        if (prev > 0 && (now - prev) < windowMs) {
            return false;
        }
        lastMs.store(now, std::memory_order_relaxed);
        return true;
    }

    const char* DirectionalScriptForCode(int dirCode, bool isRepeat) {
        switch (dirCode) {
            case 266: return isRepeat ? "handleControl('up', true)" : "handleControl('up', false)";
            case 267: return isRepeat ? "handleControl('down', true)" : "handleControl('down', false)";
            case 268: return isRepeat ? "handleControl('left', true)" : "handleControl('left', false)";
            case 269: return isRepeat ? "handleControl('right', true)" : "handleControl('right', false)";
            default:  return nullptr;
        }
    }

    bool ContainsKeyCode(const std::vector<uint32_t>& keys, uint32_t code) {
        return std::find(keys.begin(), keys.end(), code) != keys.end();
    }

    uint32_t GamepadMaskToKeyCode(uint32_t mask) {
        switch (mask) {
            case 0x0001: return 266;
            case 0x0002: return 267;
            case 0x0004: return 268;
            case 0x0008: return 269;
            case 0x0010: return 270;
            case 0x0020: return 271;
            case 0x0040: return 272;
            case 0x0080: return 273;
            case 0x0100: return 274;
            case 0x0200: return 275;
            case 0x1000: return 276;
            case 0x2000: return 277;
            case 0x4000: return 278;
            case 0x8000: return 279;
            case 0x0009: return 280;
            case 0x000A: return 281;
            default:     return 0;
        }
    }

    uint32_t GetSkyrimKeyCode(const RE::ButtonEvent* button) {
        if (!button) return 0;
        const uint32_t id = button->GetIDCode();
        switch (button->GetDevice()) {
            case RE::INPUT_DEVICE::kKeyboard:
                return (id > 0 && id < 256) ? id : 0;
            case RE::INPUT_DEVICE::kMouse:
                return (id < 10) ? (256u + id) : 0;
            case RE::INPUT_DEVICE::kGamepad:
                return GamepadMaskToKeyCode(id);
            default:
                return 0;
        }
    }

    void SetCrosshairTarget(bool hasTarget) {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto hudMenu = ui->GetMenu(RE::HUDMenu::MENU_NAME);
        if (!hudMenu || !hudMenu->uiMovie) return;
        RE::GFxValue args[6]{hasTarget, "", false, true, false, false};
        hudMenu->uiMovie->Invoke("_root.HUDMovieBaseInstance.SetCrosshairTarget", nullptr, args, 6);
    }

    std::string GetPagePath() {
        wchar_t buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, buf, MAX_PATH);
        auto p = std::filesystem::path(buf).parent_path()
               / L"Data" / L"Magelight" / L"OStimPrism" / L"views" / L"OStim" / L"index.html";
        const auto u = p.u8string();
        return std::string(reinterpret_cast<const char*>(u.data()), u.size());
    }
}

bool PrismaUIManager::HasFocus() const {
    if (hasUIMode.load()) return true;
    return magelight && modId != 0 && magelight->GetUIModeOwner() == modId;
}

void PrismaUIManager::SetFocus(bool shouldFocus) {
    if (!IsViewValid()) return;

    if (shouldFocus) {
        std::uint32_t flags = MAGELIGHT_API::kUIModeFlagNone;
        if (magelight->hostVersionNumber >= 10100 && magelight->QueryCapability("uimodecancel") == 1) {
            flags |= MAGELIGHT_API::kUIModeFlagReclaim;
        }
        auto res = magelight->RequestUIMode(view, flags);
        if (res != MAGELIGHT_API::Result::Ok) {
            SKSE::log::warn("Magelight RequestUIMode refused ({}): {}",
                static_cast<int>(res), magelight->GetLastErrorMessage(modId));
        }
    } else {
        if (magelight->hostVersionNumber >= 10100 && magelight->QueryCapability("uimodecancel") == 1) {
            magelight->CancelUIModeRequest(modId);
        }
        magelight->ReleaseUIMode(modId);
        hasUIMode.store(false);
    }
}

void PrismaUIManager::ToggleInspector() {
    if (!IsViewValid()) return;
    if (magelight->hostVersionNumber >= 2800 && magelight->QueryCapability("inspector") == 1) {
        bool visible = false;
        magelight->IsInspectorVisible(view, &visible);
        magelight->ShowInspector(view, !visible);
    }
}

void PrismaUIManager::ToggleFocusDebounced() {
    if (!TryConsumeDebounce(lastFocusToggleMs)) {
        return;
    }
    SetFocus(!HasFocus());
}

void PrismaUIManager::ToggleInspectorDebounced() {
    if (!TryConsumeDebounce(lastInspectorToggleMs)) {
        return;
    }
    ToggleInspector();
}

bool PrismaUIManager::IsMagelightCapturingGamepad() const {
    if (!IsViewValid()) {
        return false;
    }
    if (magelight->hostVersionNumber >= 10600 && magelight->QueryCapability("gamepad") == 1) {
        MAGELIGHT_API::GamepadState state{};
        state.size = sizeof(state);
        if (magelight->GetViewGamepadState(view, &state) == MAGELIGHT_API::Result::Ok) {
            return state.capturing;
        }
    }
    return false;
}

void PrismaUIManager::ResetFocusedInputState() {
    wasFocusedLastPoll = false;
    prevPolledKeyDown.clear();
    prevXInputMask = 0xFFFFFFFFu;
    xinputRepeatDir = 0;
    xinputNextRepeatMs = 0;
    xinputRightStickAcc = 0.0f;
}

void PrismaUIManager::EndUnfocusedHold() {
    if (activeUnfocusedHoldKey != 0) {
        activeUnfocusedHoldKey = 0;
        if (IsViewValid()) {
            magelight->InvokeJS(view, "handleControlEnd()");
        }
    }
}

void PrismaUIManager::PollFocusedInput(int64_t nowMs) {
    if (!IsViewValid() || currentThreadID == INVALID_THREAD_ID || openMenuCount > 0 || hiddenByMenu || !IsGameWindowForeground()) {
        ResetFocusedInputState();
        return;
    }

    const bool focused = HasFocus() || hasKeyFocus.load();
    const bool firstFocusedTick = focused && !wasFocusedLastPoll;
    auto* settings = Settings::GetSingleton();

    auto pollHotkeyList = [&](const std::vector<uint32_t>& keys, auto onTrigger) {
        for (uint32_t code : keys) {
            if (code == 0 || code >= 266) {
                continue;
            }
            const bool down = IsScanCodePhysicallyDown(code);
            if (!focused || firstFocusedTick) {
                prevPolledKeyDown[code] = down;
                continue;
            }
            const bool wasDown = prevPolledKeyDown[code];
            prevPolledKeyDown[code] = down;
            if (!hiddenByUser && down && !wasDown) {
                ResetIdleTimer();
                onTrigger();
            }
        }
    };

    pollHotkeyList(settings->toggleFocusKeys, [this]() { ToggleFocusDebounced(); });
    pollHotkeyList(settings->toggleInspectorKeys, [this]() { ToggleInspectorDebounced(); });

    if (!focused || IsMagelightCapturingGamepad()) {
        prevXInputMask = 0xFFFFFFFFu;
        xinputRepeatDir = 0;
        xinputNextRepeatMs = 0;
        xinputRightStickAcc = 0.0f;
        wasFocusedLastPoll = focused;
        return;
    }

    REX::W32::XINPUT_STATE xstate{};
    bool gotPad = false;
    if (activeXInputSlot >= 0) {
        gotPad = QueryXInputState(static_cast<DWORD>(activeXInputSlot), xstate);
        if (!gotPad) {
            activeXInputSlot = -1;
        }
    }
    if (!gotPad && (nowMs - lastXInputProbeMs >= 1000)) {
        lastXInputProbeMs = nowMs;
        for (int slot = 0; slot < 4; ++slot) {
            if (QueryXInputState(static_cast<DWORD>(slot), xstate)) {
                activeXInputSlot = slot;
                gotPad = true;
                break;
            }
        }
    }
    if (!gotPad) {
        prevXInputMask = 0xFFFFFFFFu;
        xinputRepeatDir = 0;
        xinputNextRepeatMs = 0;
        xinputRightStickAcc = 0.0f;
        wasFocusedLastPoll = focused;
        return;
    }

    const uint32_t mask = BuildXInputMask(xstate.gamepad);
    if (firstFocusedTick || prevXInputMask == 0xFFFFFFFFu) {
        prevXInputMask = mask;
        xinputRepeatDir = 0;
        xinputNextRepeatMs = 0;
        xinputRightStickAcc = 0.0f;
        wasFocusedLastPoll = focused;
        return;
    }

    const uint32_t pressedMask = mask & ~prevXInputMask;
    prevXInputMask = mask;

    if (pressedMask != 0) {
        ResetIdleTimer();
        auto* dp = OStimDataProvider::GetSingleton();
        const bool isPlayerThread = (currentThreadID == 0);

        for (uint32_t bit = 0; bit < 16; ++bit) {
            if ((pressedMask & (1u << bit)) == 0) {
                continue;
            }
            const uint32_t code = 266u + bit;

            if (!hiddenByUser && ContainsKeyCode(settings->toggleFocusKeys, code)) {
                ToggleFocusDebounced();
                wasFocusedLastPoll = focused;
                return;
            }
            if (!hiddenByUser && ContainsKeyCode(settings->toggleInspectorKeys, code)) {
                ToggleInspectorDebounced();
                continue;
            }

            bool handledCustom = false;
            if (hasCachedKeys) {
                if (code == static_cast<uint32_t>(cachedKeys.keyEnd)) {
                    SetFocus(false);
                    if (isPlayerThread) {
                        dp->StopThread(0);
                        SKSE::log::info("XInput fallback: endScene stopping player thread");
                    }
                    wasFocusedLastPoll = focused;
                    return;
                }
                if (code == static_cast<uint32_t>(cachedKeys.keyHideUI)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::KEY_HIDE);
                    ToggleHideUI();
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keySearch)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::SearchMenu);
                    if (!hiddenByUser) {
                        magelight->InvokeJS(view, "showMenu('searchMenu')");
                    }
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keyAlignment)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::AlignMenu);
                    if (!hiddenByUser) {
                        magelight->InvokeJS(view, "showMenu('alignMenu')");
                    }
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keySpeedUp)) {
                    if (isPlayerThread) {
                        dp->AdjustSpeed(0, 1);
                        UpdateThreadStatus();
                    }
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keySpeedDown)) {
                    if (isPlayerThread) {
                        dp->AdjustSpeed(0, -1);
                        UpdateThreadStatus();
                    }
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keyAutoMode)) {
                    if (isPlayerThread) {
                        dp->ToggleAutoMode(0);
                        UpdateThreadStatus();
                    }
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keyYes)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::Yes);
                    magelight->InvokeJS(view, "handleControl('yes')");
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keyToggle)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::Toggle);
                    magelight->InvokeJS(view, "handleControl('toggle')");
                    handledCustom = true;
                } else if (code == static_cast<uint32_t>(cachedKeys.keyPullOut)) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::No);
                    magelight->InvokeJS(view, "handleControl('no')");
                    handledCustom = true;
                }
            }

            if (!handledCustom) {
                const bool isCustomDir = hasCachedKeys && (
                    code == static_cast<uint32_t>(cachedKeys.keyUp) ||
                    code == static_cast<uint32_t>(cachedKeys.keyDown) ||
                    code == static_cast<uint32_t>(cachedKeys.keyLeft) ||
                    code == static_cast<uint32_t>(cachedKeys.keyRight));
                if (!isCustomDir) {
                    if (code == 276) {
                        NoteDirectControl(OstimNG_API::Thread::Controls::Yes);
                        magelight->InvokeJS(view, "handleControl('yes')");
                    } else if (code == 277) {
                        magelight->InvokeJS(view, "handleControl('esc')");
                    } else if (code == 278) {
                        NoteDirectControl(OstimNG_API::Thread::Controls::Toggle);
                        magelight->InvokeJS(view, "handleControl('toggle')");
                    }
                }
            }
        }
    }

    auto isMaskPressed = [mask](uint32_t code) {
        return code >= 266u && code <= 281u && (mask & (1u << (code - 266u))) != 0;
    };

    auto isDirectionHeld = [&](int dirCode) {
        if (hasCachedKeys) {
            uint32_t customCode = 0;
            switch (dirCode) {
                case 266: customCode = static_cast<uint32_t>(cachedKeys.keyUp); break;
                case 267: customCode = static_cast<uint32_t>(cachedKeys.keyDown); break;
                case 268: customCode = static_cast<uint32_t>(cachedKeys.keyLeft); break;
                case 269: customCode = static_cast<uint32_t>(cachedKeys.keyRight); break;
                default:  break;
            }
            if (isMaskPressed(customCode)) {
                return true;
            }
        }
        return isMaskPressed(static_cast<uint32_t>(dirCode));
    };

    int dir = 0;
    if (xinputRepeatDir >= 266 && xinputRepeatDir <= 269 && isDirectionHeld(xinputRepeatDir)) {
        dir = xinputRepeatDir;
    }
    if (dir == 0 && hasCachedKeys) {
        if (isMaskPressed(static_cast<uint32_t>(cachedKeys.keyUp)))         dir = 266;
        else if (isMaskPressed(static_cast<uint32_t>(cachedKeys.keyDown)))  dir = 267;
        else if (isMaskPressed(static_cast<uint32_t>(cachedKeys.keyLeft)))  dir = 268;
        else if (isMaskPressed(static_cast<uint32_t>(cachedKeys.keyRight))) dir = 269;
    }
    if (dir == 0) {
        if (isMaskPressed(266))      dir = 266;
        else if (isMaskPressed(267)) dir = 267;
        else if (isMaskPressed(268)) dir = 268;
        else if (isMaskPressed(269)) dir = 269;
    }
    if (dir == 0) {
        const float lx = static_cast<float>(xstate.gamepad.thumbLX);
        const float ly = static_cast<float>(xstate.gamepad.thumbLY);
        if (std::hypot(lx, ly) >= static_cast<float>(REX::W32::XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)) {
            const bool horizontal = std::fabs(lx) > std::fabs(ly) * 1.15f;
            dir = horizontal ? (lx > 0.0f ? 269 : 268) : (ly > 0.0f ? 266 : 267);
        }
    }

    auto noteDirControl = [this](int dirCode) {
        switch (dirCode) {
            case 266: NoteDirectControl(OstimNG_API::Thread::Controls::Up); break;
            case 267: NoteDirectControl(OstimNG_API::Thread::Controls::Down); break;
            case 268: NoteDirectControl(OstimNG_API::Thread::Controls::Left); break;
            case 269: NoteDirectControl(OstimNG_API::Thread::Controls::Right); break;
            default:  break;
        }
    };

    if (dir == 0) {
        xinputRepeatDir = 0;
        xinputNextRepeatMs = 0;
    } else if (dir != xinputRepeatDir) {
        xinputRepeatDir = dir;
        xinputNextRepeatMs = nowMs + 450;
        ResetIdleTimer();
        noteDirControl(dir);
        if (const char* js = DirectionalScriptForCode(dir, false)) {
            magelight->InvokeJS(view, js);
        }
    } else if (nowMs >= xinputNextRepeatMs) {
        xinputNextRepeatMs = nowMs + 115;
        ResetIdleTimer();
        noteDirControl(dir);
        if (const char* js = DirectionalScriptForCode(dir, true)) {
            magelight->InvokeJS(view, js);
        }
    }

    const float rawRy = static_cast<float>(xstate.gamepad.thumbRY);
    if (std::fabs(rawRy) < static_cast<float>(REX::W32::XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)) {
        xinputRightStickAcc = 0.0f;
    } else {
        const float ry = std::clamp(rawRy / 32767.0f, -1.0f, 1.0f);
        xinputRightStickAcc += ry * 0.025f * 10.0f;
        while (std::fabs(xinputRightStickAcc) >= 1.0f) {
            const int step = (xinputRightStickAcc > 0.0f) ? 1 : -1;
            xinputRightStickAcc -= static_cast<float>(step);
            ResetIdleTimer();
            magelight->InvokeJS(view, step > 0 ? "handleControl('up', true)" : "handleControl('down', true)");
        }
    }

    wasFocusedLastPoll = focused;
}

void PrismaUIManager::ToggleHideUI() {
    hiddenByUser = !hiddenByUser;
    if (!hiddenByUser) {
        // User is un-hiding: clear idle state and give a fresh window
        hiddenByIdle = false;
        ResetIdleTimer();
    }
    SKSE::log::info("keyHideUI toggled: hiddenByUser={}", hiddenByUser);
    ApplyVisibility();
}

void PrismaUIManager::StartListeningInput() {
    if (isListeningInput) return;
    auto deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (deviceManager) {
        deviceManager->AddEventSink(this);
        isListeningInput = true;
        SKSE::log::info("Started listening for input events");
    }
}

void PrismaUIManager::StopListeningInput() {
    if (!isListeningInput) return;
    auto deviceManager = RE::BSInputDeviceManager::GetSingleton();
    if (deviceManager) {
        deviceManager->RemoveEventSink(this);
        isListeningInput = false;
        SKSE::log::info("Stopped listening for input events");
    }
}

void PrismaUIManager::StartListeningModEvents() {
    if (isListeningModEvents) return;
    auto* source = SKSE::GetModCallbackEventSource();
    if (source) {
        source->AddEventSink(static_cast<RE::BSTEventSink<SKSE::ModCallbackEvent>*>(this));
        isListeningModEvents = true;
        SKSE::log::info("Started listening for mod callback events");
    }
}

void PrismaUIManager::StartListeningMenuEvents() {
    if (isListeningMenuEvents) return;
    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->AddEventSink(static_cast<RE::BSTEventSink<RE::MenuOpenCloseEvent>*>(this));
        isListeningMenuEvents = true;
        SKSE::log::info("Started listening for menu open/close events");
    }
}

void PrismaUIManager::StopListeningMenuEvents() {
    if (!isListeningMenuEvents) return;
    auto ui = RE::UI::GetSingleton();
    if (ui) {
        ui->RemoveEventSink(static_cast<RE::BSTEventSink<RE::MenuOpenCloseEvent>*>(this));
        isListeningMenuEvents = false;
        openMenuCount = 0;
        hiddenByMenu = false;
        SKSE::log::info("Stopped listening for menu open/close events");
    }
}

RE::BSEventNotifyControl PrismaUIManager::ProcessEvent(
    const SKSE::ModCallbackEvent* a_event,
    RE::BSTEventSource<SKSE::ModCallbackEvent>* /*a_source*/)
{
    if (!a_event) return RE::BSEventNotifyControl::kContinue;
    if (a_event->eventName != "ostim_actor_orgasm") return RE::BSEventNotifyControl::kContinue;
    // Only handle the player thread (threadID is passed as numArg)
    if (static_cast<int>(a_event->numArg) != 0) return RE::BSEventNotifyControl::kContinue;
    if (!a_event->sender) return RE::BSEventNotifyControl::kContinue;

    RE::FormID actorID = a_event->sender->GetFormID();
    SKSE::log::info("Orgasm event received for actor {:08X}", actorID);

    // Proxy the event to the Magelight UI — it owns the excitement display logic
    SKSE::GetTaskInterface()->AddTask([this, actorID]() {
        if (!IsViewValid()) return;

        int actorIndex = OStimDataProvider::GetSingleton()->GetActorIndexByFormID(currentThreadID, actorID);
        if (actorIndex < 0) {
            SKSE::log::warn("Orgasm: actor {:08X} not found in thread {}", actorID, currentThreadID);
            return;
        }

        std::string script = "onActorOrgasm(" + std::to_string(actorIndex) + ")";
        magelight->InvokeJS(view, script.c_str());
        SKSE::log::info("Proxied orgasm event to UI for actor index {}", actorIndex);
    });

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl PrismaUIManager::ProcessEvent(
    const RE::MenuOpenCloseEvent* a_event,
    RE::BSTEventSource<RE::MenuOpenCloseEvent>* /*a_source*/)
{
    if (!a_event) return RE::BSEventNotifyControl::kContinue;

    // Only hide for menus listed in Settings::trackedMenus
    const auto& trackedMenus = Settings::GetSingleton()->trackedMenus;
    bool tracked = std::any_of(trackedMenus.begin(), trackedMenus.end(),
        [&](const std::string& name) { return a_event->menuName == std::string_view(name); });

    if (!tracked) return RE::BSEventNotifyControl::kContinue;

    if (a_event->opening) {
        openMenuCount++;
    } else {
        if (openMenuCount > 0) openMenuCount--;
    }

    if (openMenuCount > 0 && !hiddenByMenu) {
        hiddenByMenu = true;
        EndUnfocusedHold();
        if (currentThreadID != INVALID_THREAD_ID) {
            SKSE::log::info("Menu opened — hiding Magelight UI");
            ApplyVisibility();
        }
    } else if (openMenuCount == 0 && hiddenByMenu) {
        hiddenByMenu = false;
        if (currentThreadID != INVALID_THREAD_ID) {
            SKSE::log::info("All menus closed — showing Magelight UI");
            // Give a fresh idle window after returning from a menu
            hiddenByIdle = false;
            ResetIdleTimer();
            UpdateKeys();
            ApplyVisibility();
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

RE::BSEventNotifyControl PrismaUIManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_source) {
    if (!a_event || !*a_event) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // Suppress unused parameter warning
    (void)a_source;

    if (!IsViewValid()) {
        return RE::BSEventNotifyControl::kContinue;
    }

    // When the view holds Magelight UI mode (or shared keyboard focus), keyboard events are delivered directly
    // to the web page (which handles them in useControls.ts) while Magelight mutes
    // DirectInput and GetAsyncKeyState for other mods.
    if (HasFocus() || hasKeyFocus.load()) {
        EndUnfocusedHold();
        return RE::BSEventNotifyControl::kContinue;
    }

    for (auto event = *a_event; event; event = event->next) {
        if (event->GetEventType() != RE::INPUT_EVENT_TYPE::kButton) {
            continue;
        }

        auto button = event->AsButtonEvent();
        if (!button) {
            continue;
        }

        const uint32_t key = GetSkyrimKeyCode(button);
        if (key == 0 || key >= 282) {
            continue;
        }

        // Ignore hotkeys while a tracked menu (e.g. Console) is open or no scene is active
        if (currentThreadID == INVALID_THREAD_ID || openMenuCount > 0 || hiddenByMenu) {
            EndUnfocusedHold();
            continue;
        }

        // Toggle Focus (Configurable) — only when not manually hidden by user
        if (!hiddenByUser) {
            auto* settings = Settings::GetSingleton();
            if (ContainsKeyCode(settings->toggleFocusKeys, key) && button->IsDown()) {
                ResetIdleTimer();
                ToggleFocusDebounced();
            }

            // Toggle Inspector (Configurable)
            if (ContainsKeyCode(settings->toggleInspectorKeys, key) && button->IsDown()) {
                ResetIdleTimer();
                ToggleInspectorDebounced();
            }
        }

        // All OStim key bindings are handled here in ProcessEvent when unfocused.
        if (hasCachedKeys && IsViewValid()) {

            // Hardcoded keys not in OStim KeyData
            constexpr uint32_t KEY_ESCAPE = 0x01;
            constexpr uint32_t KEY_TAB    = 0x0F;

            // Check if this is an OStim-bound key — if so, count it as activity
            if (button->IsDown()) {
                const std::initializer_list<uint32_t> ostimKeys = {
                    (uint32_t)cachedKeys.keyUp,       (uint32_t)cachedKeys.keyDown,
                    (uint32_t)cachedKeys.keyLeft,     (uint32_t)cachedKeys.keyRight,
                    (uint32_t)cachedKeys.keyYes,      (uint32_t)cachedKeys.keyEnd,
                    (uint32_t)cachedKeys.keyToggle,   (uint32_t)cachedKeys.keySearch,
                    (uint32_t)cachedKeys.keyAlignment,(uint32_t)cachedKeys.keyHideUI,
                    (uint32_t)cachedKeys.keySpeedUp,  (uint32_t)cachedKeys.keySpeedDown,
                    (uint32_t)cachedKeys.keyPullOut,  (uint32_t)cachedKeys.keyAutoMode,
                    (uint32_t)cachedKeys.keyFreeCam,
                    KEY_ESCAPE, KEY_TAB,
                };
                if (std::find(ostimKeys.begin(), ostimKeys.end(), key) != ostimKeys.end()) {
                    ResetIdleTimer();
                }
            }

            // Directional/Yes keys: use handleControlStart/End for hold-to-repeat
            const char* controlStr = nullptr;
            if (key == (uint32_t)cachedKeys.keyUp) {
                controlStr = "up";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Up);
            } else if (key == (uint32_t)cachedKeys.keyDown) {
                controlStr = "down";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Down);
            } else if (key == (uint32_t)cachedKeys.keyLeft) {
                controlStr = "left";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Left);
            } else if (key == (uint32_t)cachedKeys.keyRight) {
                controlStr = "right";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Right);
            } else if (key == (uint32_t)cachedKeys.keyYes) {
                controlStr = "yes";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Yes);
            } else if (key == (uint32_t)cachedKeys.keyToggle) {
                controlStr = "toggle";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::Toggle);
            } else if (key == (uint32_t)cachedKeys.keyEnd) {
                controlStr = "no";
                if (button->IsDown()) NoteDirectControl(OstimNG_API::Thread::Controls::No);
            } else if (key == (uint32_t)cachedKeys.keyHideUI) {
                if (button->IsDown()) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::KEY_HIDE);
                    ToggleHideUI();
                }
            } else if (key == (uint32_t)cachedKeys.keySearch) {
                if (button->IsDown()) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::SearchMenu);
                    if (!hiddenByUser) {
                        SKSE::log::info("Opening Search Menu");
                        magelight->InvokeJS(view, "showMenu('searchMenu')");
                    }
                }
            } else if (key == (uint32_t)cachedKeys.keyAlignment) {
                if (button->IsDown()) {
                    NoteDirectControl(OstimNG_API::Thread::Controls::AlignMenu);
                    if (!hiddenByUser) {
                        SKSE::log::info("Opening Alignment Menu");
                        magelight->InvokeJS(view, "showMenu('alignMenu')");
                    }
                }
            }
            else if (key == KEY_ESCAPE) controlStr = "esc";
            else if (key == KEY_TAB)    controlStr = "tab";

            if (controlStr) {
                if (button->IsDown()) {
                    const bool canHold = (key >= 266 && key <= 281) || IsScanCodePhysicallyDown(key);
                    if (canHold) {
                        activeUnfocusedHoldKey = key;
                        std::string script = "handleControlStart('" + std::string(controlStr) + "')";
                        magelight->InvokeJS(view, script.c_str());
                    } else {
                        EndUnfocusedHold();
                        std::string script = "handleControl('" + std::string(controlStr) + "', false)";
                        magelight->InvokeJS(view, script.c_str());
                    }
                } else if (!button->IsPressed()) {
                    if (activeUnfocusedHoldKey == key) {
                        EndUnfocusedHold();
                    }
                }
            }
        }
    }

    return RE::BSEventNotifyControl::kContinue;
}

void PrismaUIManager::Initialize() {
    if (initialized) {
        return;
    }

    auto api = MAGELIGHT_API::RequestApi4();
    if (!api) {
        SKSE::log::info("Magelight UI not available");
        return;
    }

    MAGELIGHT_API::ModDesc desc{};
    desc.size = sizeof(desc);
    desc.modId = "OStimPrism";
    desc.displayName = "OStim Prism";
    desc.modVersion = MAGELIGHT_API::PackVersion(0, 0, 1);
    desc.minHostVersion = MAGELIGHT_API::PackVersion(0, 28, 0);
    desc.callbackThread = MAGELIGHT_API::CallbackThread::GameThread;
    desc.onEvent = &PrismaUIManager::OnMagelightEvent;
    desc.onLog = nullptr;
    desc.user = this;
    desc.sessionName = nullptr;

    MAGELIGHT_API::ModId registeredMod = 0;
    auto res = api->RegisterMod(&desc, &registeredMod);
    if (res != MAGELIGHT_API::Result::Ok || !registeredMod) {
        SKSE::log::error("Failed to register OStimPrism with Magelight UI (result={})", static_cast<int>(res));
        return;
    }

    magelight = api;
    modId = registeredMod;
    initialized = true;
    SKSE::log::info("Magelight UI (host {}) initialized successfully for OStim Prism (modId={})",
        magelight->hostVersion ? magelight->hostVersion : "?", modId);

    // Create a single persistent view (hidden until a scene starts).
    // Magelight pauses hidden views completely (0 CPU/GPU cost while hidden).
    const std::string pagePath = GetPagePath();
    MAGELIGHT_API::ViewDesc vd{};
    vd.size = sizeof(vd);
    vd.name = "OStim";
    vd.htmlPath = pagePath.c_str();
    vd.anchor = MAGELIGHT_API::Anchor::TopLeft;
    vd.fullscreen = true;
    vd.clickThrough = false;
    vd.startVisible = false;
    vd.layer = MAGELIGHT_API::Layer::Panel;
    vd.uiScale = 0.0f;
    vd.onDomReady = nullptr; // Handled on the game thread via Event::ViewDomReady

    MAGELIGHT_API::ViewId newView = 0;
    res = magelight->CreateViewEx(modId, &vd, &newView);
    if (res != MAGELIGHT_API::Result::Ok || !newView) {
        SKSE::log::error("Failed to create Magelight view ({}): {}",
            static_cast<int>(res), magelight->GetLastErrorMessage(modId));
        return;
    }

    view = newView;
    isCurrentlyVisible = false;
    magelight->RegisterJSListener(view, "sendAction", &PrismaUIManager::OnAction);

    if (magelight->hostVersionNumber >= 2800 && magelight->QueryCapability("escapecapture") == 1) {
        magelight->SetEscapeCapture(view, true);
    }

    if (magelight->hostVersionNumber >= 10600 && magelight->QueryCapability("gamepad") == 1) {
        MAGELIGHT_API::GamepadConfig pad{};
        pad.size = sizeof(pad);
        pad.enabled = true;
        pad.style = MAGELIGHT_API::kGamepadStyleNavigation;
        pad.allowCursor = true;
        magelight->SetViewGamepad(view, &pad);
    }

    if (magelight->hostVersionNumber >= 3111 && magelight->QueryCapability("prepaint") == 1) {
        magelight->SetViewPrepaint(view, true);
    }

    if (magelight->hostVersionNumber >= 10900 && magelight->QueryCapability("focusShare") == 1) {
        magelight->SetViewFocusSharing(view, MAGELIGHT_API::kFocusShareGive | MAGELIGHT_API::kFocusShareTake);
    }

    SKSE::log::info("Magelight persistent view created: {} ({})", newView, pagePath);

    StartListeningModEvents();
    StartListeningMenuEvents();
}

void PrismaUIManager::Show() {
    if (!IsViewValid()) {
        return;
    }

    hiddenByUser = false;
    hiddenByIdle = false;
    ResetFocusedInputState();
    lastExcitementPollMs = 0;
    ResetIdleTimer();
    ApplyVisibility();

    magelight->InvokeJS(view, "showMenu('navigation')");
    UpdateSettings();
    UpdateNavigation();
    UpdateExcitements(true);
    UpdateOptions();
    UpdateThreadStatus();
    UpdateKeys();

    StartListeningInput();
    StartPolling();
}

void PrismaUIManager::Hide() {
    StopPolling();
    StopListeningInput();

    currentThreadID = INVALID_THREAD_ID;
    hiddenByUser = false;
    hiddenByIdle = false;
    lastActivityTime.store(0);
    activeUnfocusedHoldKey = 0;
    ResetFocusedInputState();

    if (IsViewValid()) {
        magelight->InvokeJS(view, "handleControlEnd()");
        magelight->InvokeJS(view, "showMenu('navigation')");
    }

    ApplyVisibility();
}

void PrismaUIManager::StartPolling() {
    if (isPolling) return;
    isPolling = true;
    uint64_t gen = ++pollGeneration;

    // Ensure the polling loop starts on the main thread
    SKSE::GetTaskInterface()->AddTask([this, gen]() {
        if (isPolling && pollGeneration.load() == gen) {
            PollUpdate(gen);
        }
    });

    SKSE::log::info("Started polling for excitement updates (100ms interval)");
}

void PrismaUIManager::StopPolling() {
    if (!isPolling) return;
    isPolling = false;
    ++pollGeneration;
    SKSE::log::info("Stopped polling for excitement updates");
}

void PrismaUIManager::ResetIdleTimer() {
    auto now = std::chrono::steady_clock::now().time_since_epoch();
    lastActivityTime.store(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
    if (hiddenByIdle) {
        SKSE::log::info("Activity detected — showing Magelight UI after idle");
        hiddenByIdle = false;
        ApplyVisibility();
    }
}

void PrismaUIManager::ApplyVisibility() {
    if (!IsViewValid()) return;
    bool shouldHide = (currentThreadID == INVALID_THREAD_ID) || hiddenByMenu || hiddenByUser || hiddenByIdle;
    if (shouldHide) {
        EndUnfocusedHold();
        if (HasFocus()) {
            SetFocus(false);
        } else {
            magelight->CancelUIModeRequest(modId);
        }
        magelight->ShowView(view, false);
        isCurrentlyVisible = false;
    } else {
        bool wasHidden = !isCurrentlyVisible;
        magelight->ShowView(view, true);
        isCurrentlyVisible = true;
        if (wasHidden) {
            UpdateExcitements();
        }
    }
}

void PrismaUIManager::CheckIdleTimeout() {
    // Don't accumulate idle state while a menu is open, user hid the UI, or UI mode is active
    if (!IsViewValid() || currentThreadID == INVALID_THREAD_ID || hiddenByMenu || hiddenByUser) return;
    if (HasFocus() || hasKeyFocus.load()) {
        ResetIdleTimer();
        return;
    }

    auto now = std::chrono::steady_clock::now().time_since_epoch();
    int64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    int64_t lastMs = lastActivityTime.load();
    int64_t idleMs = nowMs - lastMs;

    int64_t idleTimeoutMs = static_cast<int64_t>(Settings::GetSingleton()->idleTimeoutSec) * 1000;
    if (idleTimeoutMs > 0 && idleMs >= idleTimeoutMs && !hiddenByIdle) {
        SKSE::log::info("UI idle for {}ms — hiding Magelight UI", idleMs);
        hiddenByIdle = true;
        ApplyVisibility();
    }
}

void PrismaUIManager::PollUpdate(uint64_t generation) {
    if (!isPolling || pollGeneration.load() != generation) return;

    if (currentThreadID == INVALID_THREAD_ID) {
        isPolling = false;
        return;
    }

    const int64_t nowMs = SteadyNowMs();
    PollFocusedInput(nowMs);

    if (!HasFocus() && !hasKeyFocus.load() && activeUnfocusedHoldKey != 0) {
        if (!IsGameWindowForeground() || (activeUnfocusedHoldKey < 266 && !IsScanCodePhysicallyDown(activeUnfocusedHoldKey))) {
            EndUnfocusedHold();
        }
    }

    if (isCurrentlyVisible && (nowMs - lastExcitementPollMs >= 100)) {
        lastExcitementPollMs = nowMs;
        UpdateExcitements();
    }
    CheckIdleTimeout();

    const int sleepMs = (HasFocus() || hasKeyFocus.load()) ? 25 : 100;

    // Sleep in a background thread to avoid blocking the main game loop
    std::thread([this, generation, sleepMs]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));

        // Re-schedule PollUpdate on the main thread
        if (isPolling && pollGeneration.load() == generation) {
            SKSE::GetTaskInterface()->AddTask([this, generation]() {
                if (isPolling && pollGeneration.load() == generation) {
                    PollUpdate(generation);
                }
            });
        }
    }).detach();
}

void PrismaUIManager::UpdateKeys()
{
    if (!IsViewValid()) return;

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) return;

    auto keys = dataProvider->GetKeyData();
    cachedKeys = keys;
    hasCachedKeys = true;

    const auto* settings = Settings::GetSingleton();

    json j = {        
        {"keyUp", keys.keyUp},
        {"keyDown", keys.keyDown},
        {"keyLeft", keys.keyLeft},
        {"keyRight", keys.keyRight},
        {"keyYes", keys.keyYes},
        {"keyEnd", keys.keyEnd},
        {"keyToggle", keys.keyToggle},
        {"keySearch", keys.keySearch},
        {"keyAlignment", keys.keyAlignment},
        {"keySceneStart", keys.keySceneStart},
        {"keyNpcSceneStart", keys.keyNpcSceneStart},
        {"keySpeedUp", keys.keySpeedUp},
        {"keySpeedDown", keys.keySpeedDown},
        {"keyPullOut", keys.keyPullOut},
        {"keyAutoMode", keys.keyAutoMode},
        {"keyFreeCam", keys.keyFreeCam},
        {"keyHideUI", keys.keyHideUI},
        {"toggleFocusKeys", settings->toggleFocusKeys},
        {"toggleInspectorKeys", settings->toggleInspectorKeys}
    };
    
    std::string script = "updateKeys(" + j.dump() + ")";
    magelight->InvokeJS(view, script.c_str());
    SKSE::log::info("Sent key update to UI");
}

void PrismaUIManager::UpdateSettings()
{
    if (!IsViewValid()) return;

    auto* settings = Settings::GetSingleton();
    settings->Load();

    json j = {
        {"enableGridMenu", settings->enableGridMenu},
        {"enablePortraits", settings->enablePortraits},
        {"gridColumns", settings->gridColumns},
        {"gridRows", settings->gridRows},
        {"mainBlockX", settings->mainBlockX},
        {"mainBlockY", settings->mainBlockY},
        {"mainBlockScale", settings->mainBlockScale},
        {"excitementBlockX", settings->excitementBlockX},
        {"excitementBlockY", settings->excitementBlockY},
        {"excitementBlockScale", settings->excitementBlockScale}
    };

    std::string script = "updateSettings(" + j.dump() + ")";
    magelight->InvokeJS(view, script.c_str());
    SKSE::log::info("Sent settings update to UI (enableGridMenu={}, gridColumns={}, gridRows={})",
        settings->enableGridMenu, settings->gridColumns, settings->gridRows);
}

void PrismaUIManager::UpdateExcitements(bool forceResendPortraits) {
    if (!IsViewValid()) {
        return;
    }

    // Also refresh thread status (speed, control flags) on every excitement poll
    UpdateThreadStatus();

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    // currentThreadID is 0 for player thread (validated in OnThreadEvent)
    auto actors = dataProvider->GetActorExcitements(currentThreadID);
    
    json actorsJson = json::array();
    for (const auto& actor : actors) {
        actorsJson.push_back({
            {"formId", actor.formId},
            {"name", actor.name},
            {"excitementProgress", actor.excitementProgress},
            {"staminaProgress", actor.staminaProgress},
            {"gender", actor.gender},
            {"timesClimaxed", actor.timesClimaxed},
            {"additionalProgress", actor.additionalProgress}
        });
    }

    std::string script = "updateExcitements(" + actorsJson.dump() + ")";
    magelight->InvokeJS(view, script.c_str());

    PortraitManager::GetSingleton()->SyncActors(actors, forceResendPortraits);
}

void PrismaUIManager::SendActorPortrait(uint32_t formId, const std::string& dataUrl) {
    if (!IsViewValid()) {
        return;
    }
    std::string script = "updateActorPortrait(" + std::to_string(formId) + "," + json(dataUrl).dump() + ")";
    magelight->InvokeJS(view, script.c_str());
}

void PrismaUIManager::ClearActorPortraits() {
    if (!IsViewValid()) {
        return;
    }
    magelight->InvokeJS(view, "clearActorPortraits()");
}

void PrismaUIManager::UpdateNavigation() {
    SKSE::log::info("UpdateNavigation called");

    if (!IsViewValid()) {
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    // currentThreadID is 0 for player thread (validated in OnThreadEvent)
    auto options = dataProvider->GetNavigationOptions(currentThreadID);

    json navJson = json::array();
    for (const auto& nav : options) {
        navJson.push_back({
            {"id", nav.id},
            {"iconPath", nav.iconPath},
            {"iconData", nav.iconData},
            {"description", nav.description},
            {"destination", nav.destination}
        });
    }

    std::string jsonStr = navJson.dump();
    std::string script = "updateNavigation(" + jsonStr + ")";
    SKSE::log::info("UpdateNavigation sending {} options", options.size());
    magelight->InvokeJS(view, script.c_str());
}

void PrismaUIManager::UpdateOptions() {
    SKSE::log::info("UpdateOptions called");

    if (!IsViewValid()) {
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    auto items = dataProvider->GetSceneOptions();

    json optionsJson = json::array();
    for (const auto& item : items) {
        optionsJson.push_back({
            {"id", item.id},
            {"iconPath", item.icon}, // UI expects iconPath
            {"iconData", item.iconData},
            {"border", item.border},
            {"description", item.description}
        });
    }

    std::string script = "updateOptions(" + optionsJson.dump() + ")";
    SKSE::log::info("UpdateOptions sending {} items", items.size());
    magelight->InvokeJS(view, script.c_str());
}

void PrismaUIManager::UpdateThreadStatus() {
    if (!IsViewValid()) {
        return;
    }

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("OStim data provider not connected");
        return;
    }

    auto status = dataProvider->GetThreadStatus(currentThreadID);

    json statusJson = {
        {"autoControl",   status.autoControl},
        {"manualControl", status.manualControl},
        {"locked",        status.locked},
        {"currentSpeed",  status.currentSpeed},
        {"minSpeed",      status.minSpeed},
        {"maxSpeed",      status.maxSpeed}
    };

    std::string statusScript = "updateThreadStatus(" + statusJson.dump() + ")";
    magelight->InvokeJS(view, statusScript.c_str());
}

void PrismaUIManager::NoteDirectControl(OstimNG_API::Thread::Controls control) {
    const auto idx = static_cast<size_t>(control);
    if (idx < lastDirectControlMs.size()) {
        lastDirectControlMs[idx].store(SteadyNowMs(), std::memory_order_relaxed);
    }
}

bool PrismaUIManager::WasRecentlyHandledDirectly(OstimNG_API::Thread::Controls control, int64_t eventTimeMs) const {
    const auto idx = static_cast<size_t>(control);
    if (idx >= lastDirectControlMs.size()) {
        return false;
    }
    const int64_t lastMs = lastDirectControlMs[idx].load(std::memory_order_relaxed);
    return lastMs > 0 && std::abs(eventTimeMs - lastMs) < 200;
}

void PrismaUIManager::OnControlEvent(OstimNG_API::Thread::Controls controlType, uint32_t threadID, void* /*userData*/) {
    if (threadID != 0) {
        return;
    }

    const int64_t eventTimeMs = SteadyNowMs();
    SKSE::GetTaskInterface()->AddTask([controlType, threadID, eventTimeMs]() {
        if (threadID != 0) {
            return;
        }

        auto* manager = GetSingleton();
        if (!manager || !manager->IsViewValid()) {
            return;
        }

        if (manager->currentThreadID != 0 || manager->openMenuCount > 0 || manager->hiddenByMenu) {
            return;
        }

        if ((manager->HasFocus() || manager->hasKeyFocus.load()) && manager->IsMagelightCapturingGamepad()) {
            return;
        }

        if (manager->WasRecentlyHandledDirectly(controlType, eventTimeMs)) {
            return;
        }

        manager->ResetIdleTimer();
        manager->UpdateKeys();

        using Controls = OstimNG_API::Thread::Controls;
        switch (controlType) {
            case Controls::Up:
                manager->magelight->InvokeJS(manager->view, "handleControl('up')");
                break;
            case Controls::Down:
                manager->magelight->InvokeJS(manager->view, "handleControl('down')");
                break;
            case Controls::Left:
                manager->magelight->InvokeJS(manager->view, "handleControl('left')");
                break;
            case Controls::Right:
                manager->magelight->InvokeJS(manager->view, "handleControl('right')");
                break;
            case Controls::Yes:
                manager->magelight->InvokeJS(manager->view, "handleControl('yes')");
                break;
            case Controls::Toggle:
                manager->magelight->InvokeJS(manager->view, "handleControl('toggle')");
                break;
            case Controls::No:
                manager->magelight->InvokeJS(manager->view, "handleControl('no')");
                break;
            case Controls::Menu:
                if (!manager->hiddenByUser) {
                    manager->magelight->InvokeJS(manager->view, "showMenu('utilityOptions')");
                }
                break;
            case Controls::KEY_HIDE:
                manager->ToggleHideUI();
                break;
            case Controls::AlignMenu:
                if (!manager->hiddenByUser) {
                    manager->magelight->InvokeJS(manager->view, "showMenu('alignMenu')");
                }
                break;
            case Controls::SearchMenu:
                if (!manager->hiddenByUser) {
                    manager->magelight->InvokeJS(manager->view, "showMenu('searchMenu')");
                }
                break;
            default:
                break;
        }
    });
}

void PrismaUIManager::OnThreadEvent(OstimNG_API::Thread::ThreadEvent eventType, uint32_t threadID, void* /*userData*/) {
    // Dispatch to main thread to avoid deadlocks/crashes when calling UI or OStim APIs
    SKSE::GetTaskInterface()->AddTask([eventType, threadID]() {
        auto manager = GetSingleton();
        if (!manager) return;

        SKSE::log::info("OnThreadEvent (Main Thread): event={}, threadID={}", static_cast<int>(eventType), threadID);

        // Only handle player thread (threadID == 0)
        if (threadID != 0) {
            return;
        }

        switch (eventType) {
            case OstimNG_API::Thread::ThreadEvent::ThreadStarted:
                SKSE::log::info("Thread started: {}", threadID);
                manager->currentThreadID = threadID;
                PortraitManager::GetSingleton()->OnSceneChanged();
                SetCrosshairTarget(false);
                manager->Show();
                break;

            case OstimNG_API::Thread::ThreadEvent::ThreadEnded:
                SKSE::log::info("Thread ended: {}", threadID);
                if (manager->currentThreadID == threadID) {
                    SetCrosshairTarget(true);
                    manager->Hide();
                }
                break;

            case OstimNG_API::Thread::ThreadEvent::NodeChanged:
                SKSE::log::info("Node changed: {}", threadID);
                if (manager->currentThreadID == threadID) {
                    manager->UpdateNavigation();
                    manager->UpdateThreadStatus();
                }
                break;
        }
    });
}

void PrismaUIManager::OnMagelightEvent(const MAGELIGHT_API::EventData* ev, void* user) {
    auto* manager = static_cast<PrismaUIManager*>(user);
    if (!manager || !ev || !manager->magelight) {
        return;
    }

    switch (ev->type) {
        case MAGELIGHT_API::Event::ViewDomReady:
        case MAGELIGHT_API::Event::ViewReloaded: {
            if (ev->view != manager->view) return;
            SKSE::log::info("Magelight view {} {}: syncing state",
                ev->view, ev->type == MAGELIGHT_API::Event::ViewDomReady ? "DOM ready" : "reloaded");

            manager->UpdateSettings();
            manager->UpdateKeys();
            manager->magelight->InvokeJS(ev->view, "setGameReady()");

            if (manager->currentThreadID != INVALID_THREAD_ID) {
                manager->UpdateNavigation();
                manager->UpdateExcitements(true);
                manager->UpdateOptions();
                manager->UpdateThreadStatus();
                manager->ResetIdleTimer();
                manager->StartListeningInput();
            }
            break;
        }

        case MAGELIGHT_API::Event::UIModeEntered: {
            if (ev->view == manager->view) {
                manager->EndUnfocusedHold();
                manager->hasUIMode.store(true);
                manager->hasKeyFocus.store(true);
                manager->ResetIdleTimer();
                SKSE::log::info("Magelight UI mode entered for view {}", ev->view);
                if (manager->currentThreadID == INVALID_THREAD_ID || manager->hiddenByMenu || manager->hiddenByUser) {
                    manager->ApplyVisibility();
                }
            }
            break;
        }

        case MAGELIGHT_API::Event::UIModeExited:
        case MAGELIGHT_API::Event::UIModeRefused: {
            manager->hasUIMode.store(false);
            manager->hasKeyFocus.store(false);
            manager->ResetIdleTimer();
            SKSE::log::info("Magelight UI mode exited/refused (view={}, detail='{}')",
                ev->view, ev->detail ? ev->detail : "");
            break;
        }

        case MAGELIGHT_API::Event::KeyFocusGained: {
            if (ev->view == manager->view) {
                manager->EndUnfocusedHold();
                manager->hasKeyFocus.store(true);
                manager->ResetIdleTimer();
                SKSE::log::debug("Magelight key focus gained for view {} (from view {}, mod='{}')",
                    ev->view, ev->x, ev->detail ? ev->detail : "");
            }
            break;
        }

        case MAGELIGHT_API::Event::KeyFocusLost: {
            if (ev->view == manager->view) {
                manager->hasKeyFocus.store(false);
                SKSE::log::debug("Magelight key focus lost for view {} (to view {}, mod='{}')",
                    ev->view, ev->x, ev->detail ? ev->detail : "");
            }
            break;
        }

        case MAGELIGHT_API::Event::ViewLoadFailed: {
            SKSE::log::error("Magelight view {} failed to load: {}",
                ev->view, ev->detail ? ev->detail : "unknown error");
            break;
        }

        default:
            break;
    }
}

void PrismaUIManager::OnActionGameTask(void* user) {
    std::unique_ptr<std::string> payload(static_cast<std::string*>(user));
    if (!payload) return;
    auto* manager = GetSingleton();
    if (manager) {
        manager->HandleActionOnGameThread(*payload);
    }
}

void PrismaUIManager::OnAction(const char* data) {
    if (!data) return;
    auto* manager = GetSingleton();
    if (!manager || !manager->magelight) return;

    auto* copy = new std::string(data);
    if (manager->magelight->hostVersionNumber >= 3001) {
        if (manager->magelight->PostGameTask(&PrismaUIManager::OnActionGameTask, copy) != MAGELIGHT_API::Result::Ok) {
            delete copy;
        }
    } else {
        std::thread([copy]() {
            SKSE::GetTaskInterface()->AddTask([copy]() {
                OnActionGameTask(copy);
            });
        }).detach();
    }
}

void PrismaUIManager::HandleActionOnGameThread(const std::string& data) {
    SKSE::log::info("Magelight UI action received: {}", data);

    auto dataProvider = OStimDataProvider::GetSingleton();
    if (!dataProvider->IsConnected()) {
        SKSE::log::warn("Cannot handle action: OStim not connected");
        return;
    }

    try {
        json actionData = json::parse(data);

        std::string action = actionData.value("action", "");
        if (action.empty()) {
            SKSE::log::warn("Magelight UI action missing 'action' field");
            return;
        }

        auto sendAlignmentUpdate = [&](uint32_t threadID, uint32_t actorIndex) {
            if (!IsViewValid()) {
                return;
            }

            uint32_t actorCount = dataProvider->GetActorCount(threadID);
            if (actorCount == 0) {
                actorCount = 1;
            }

            if (actorIndex >= actorCount) {
                actorIndex = actorCount - 1;
            }

            // Get scene info
            OStimDataProvider::SearchResult sceneInfo{};
            if (!dataProvider->GetCurrentSceneInfo(threadID, sceneInfo)) {
                SKSE::log::warn("Failed to fetch scene info (threadID={})", threadID);
            }

            // Get actor info
            auto actors = dataProvider->GetActorExcitements(threadID);
            std::string actorName = "Unknown";
            std::string actorGender = "unknown";
            if (actorIndex < actors.size()) {
                actorName = actors[actorIndex].name;
                actorGender = actors[actorIndex].gender;
            }

            OStimDataProvider::AlignmentData alignData{};
            if (!dataProvider->GetActorAlignment(threadID, actorIndex, alignData)) {
                SKSE::log::warn("Failed to fetch alignment data (actorIndex={})", actorIndex);
                return;
            }

            json payload = {
                {"actorIndex", actorIndex},
                {"actorCount", actorCount},
                {"sceneId", sceneInfo.sceneId},
                {"sceneName", sceneInfo.name},
                {"actorName", actorName},
                {"actorGender", actorGender},
                {"data", {
                    {"offsetX", alignData.offsetX},
                    {"offsetY", alignData.offsetY},
                    {"offsetZ", alignData.offsetZ},
                    {"scale", alignData.scale},
                    {"rotation", alignData.rotation},
                    {"sosBend", alignData.sosBend}
                }}
            };

            SKSE::log::info("Sending alignment update: actorIndex={}, sceneId={}, actorName={}",
                actorIndex, sceneInfo.sceneId, actorName);

            std::string script = "updateAlignment(" + payload.dump() + ")";
            magelight->InvokeJS(view, script.c_str());
        };

        if (action == "navigationSelect") {
            auto payload = actionData.value("payload", json::object());
            std::string destination = payload.value("destination", "");

            if (destination.empty()) {
                SKSE::log::warn("Magelight UI navigationSelect missing destination");
                return;
            }

            // Only navigate if we're on player thread (threadID == 0)
            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->NavigateToScene(threadID, destination);
            } else {
                SKSE::log::warn("Cannot navigate: not on player thread (threadID={})", threadID);
            }
        } else if (action == "searchQuery") {
            if (!IsViewValid()) {
                return;
            }

            auto payload = actionData.value("payload", json::object());
            std::string query = payload.value("query", "");

            if (query.empty()) {
                magelight->InvokeJS(view, "updateSearchResults([])");
                return;
            }

            auto results = dataProvider->SearchScenes(query, 999);
            json resultsJson = json::array();
            for (const auto& item : results) {
                resultsJson.push_back({
                    {"sceneId", item.sceneId},
                    {"name", item.name},
                    {"actorCount", item.actorCount}
                });
            }

            std::string script = "updateSearchResults(" + resultsJson.dump() + ")";
            magelight->InvokeJS(view, script.c_str());
        } else if (action == "searchSelect") {
            auto payload = actionData.value("payload", json::object());
            std::string sceneId = payload.value("sceneId", "");

            if (sceneId.empty()) {
                SKSE::log::warn("Magelight UI searchSelect missing sceneId");
                return;
            }

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->NavigateToSearchResult(threadID, sceneId);
            } else {
                SKSE::log::warn("Cannot navigate search result: not on player thread (threadID={})", threadID);
            }
        } else if (action == "selectOption") {
            auto payload = actionData.value("payload", json::object());
            int32_t index = payload.value("index", -1);

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                // Returns true if still in options menu
                bool stillOpen = dataProvider->SelectOptionsItem(index);

                if (!stillOpen) {
                    SKSE::log::info("Options menu closed after selecting index {}", index);
                    std::string script = "showMenu('navigation')";
                    magelight->InvokeJS(view, script.c_str());
                } else {
                    // Refresh the options list (could be new page or empty if closed)
                    UpdateOptions();
                }
            } else {
                SKSE::log::warn("Cannot select option: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentInit") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = (payload.contains("actorIndex") && !payload["actorIndex"].is_null() && payload["actorIndex"].is_number()) 
                ? payload["actorIndex"].get<uint32_t>() : 0u;

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                sendAlignmentUpdate(threadID, actorIndex);
            } else {
                SKSE::log::warn("Cannot init alignment: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentSelectActor") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = (payload.contains("actorIndex") && !payload["actorIndex"].is_null() && payload["actorIndex"].is_number()) 
                ? payload["actorIndex"].get<uint32_t>() : 0u;

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                sendAlignmentUpdate(threadID, actorIndex);
            } else {
                SKSE::log::warn("Cannot select alignment actor: not on player thread (threadID={})", threadID);
            }
        } else if (action == "alignmentSet") {
            auto payload = actionData.value("payload", json::object());
            uint32_t actorIndex = (payload.contains("actorIndex") && !payload["actorIndex"].is_null() && payload["actorIndex"].is_number()) 
                ? payload["actorIndex"].get<uint32_t>() : 0u;
            std::string field = (payload.contains("field") && payload["field"].is_string()) ? payload["field"].get<std::string>() : "";
            float value = (payload.contains("value") && payload["value"].is_number()) ? payload["value"].get<float>() : 0.0f;

            if (field.empty()) {
                SKSE::log::warn("alignmentSet missing field");
                return;
            }

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID != 0) {
                SKSE::log::warn("Cannot set alignment: not on player thread (threadID={})", threadID);
                return;
            }

            OStimDataProvider::AlignmentData alignData{};
            if (!dataProvider->GetActorAlignment(threadID, actorIndex, alignData)) {
                SKSE::log::warn("Failed to fetch alignment data for set (actorIndex={})", actorIndex);
                return;
            }

            if (field == "offsetX") alignData.offsetX = value;
            else if (field == "offsetY") alignData.offsetY = value;
            else if (field == "offsetZ") alignData.offsetZ = value;
            else if (field == "scale") alignData.scale = value;
            else if (field == "rotation") alignData.rotation = value;
            else if (field == "sosBend") alignData.sosBend = value;
            else {
                SKSE::log::warn("alignmentSet unknown field: {}", field);
                return;
            }

            if (!dataProvider->SetActorAlignment(threadID, actorIndex, alignData)) {
                SKSE::log::warn("Failed to apply alignment set (actorIndex={})", actorIndex);
                return;
            }
        } else if (action == "setSpeed") {
            auto payload = actionData.value("payload", json::object());
            int32_t speed = (payload.contains("speed") && payload["speed"].is_number()) ? payload["speed"].get<int32_t>() : 1;

            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->SetSpeed(threadID, speed);
                SKSE::log::info("setSpeed: applied speed {} on thread {}", speed, threadID);
                // Echo updated status back to UI immediately
                UpdateThreadStatus();
            } else {
                SKSE::log::warn("Cannot set speed: not on player thread (threadID={})", threadID);
            }
        } else if (action == "focus") {
            auto payload = actionData.value("payload", json::object());
            bool shouldFocus = (payload.contains("shouldFocus") && payload["shouldFocus"].is_boolean()) ? payload["shouldFocus"].get<bool>() : false;
            SetFocus(shouldFocus);
        } else if (action == "toggleFocus") {
            ToggleFocusDebounced();
        } else if (action == "toggleInspector") {
            ToggleInspectorDebounced();
        } else if (action == "toggleHideUI") {
            ToggleHideUI();
        } else if (action == "endScene") {
            SetFocus(false);
            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->StopThread(0);
                SKSE::log::info("endScene: stopping player thread");
            } else {
                SKSE::log::warn("Cannot end scene: not on player thread (threadID={})", threadID);
            }
        } else if (action == "adjustSpeed") {
            auto payload = actionData.value("payload", json::object());
            int32_t delta = (payload.contains("delta") && payload["delta"].is_number()) ? payload["delta"].get<int32_t>() : 0;
            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0 && delta != 0) {
                dataProvider->AdjustSpeed(0, delta);
                UpdateThreadStatus();
            }
        } else if (action == "toggleAutoMode") {
            uint32_t threadID = dataProvider->GetCurrentThreadID();
            if (threadID == 0) {
                dataProvider->ToggleAutoMode(0);
                UpdateThreadStatus();
            }
        } else if (action == "saveLayout") {
            auto payload = actionData.value("payload", json::object());
            auto* settings = Settings::GetSingleton();
            int mainX = payload.value("mainBlockX", settings->mainBlockX);
            int mainY = payload.value("mainBlockY", settings->mainBlockY);
            int mainScale = payload.value("mainBlockScale", settings->mainBlockScale);
            int exciteX = payload.value("excitementBlockX", settings->excitementBlockX);
            int exciteY = payload.value("excitementBlockY", settings->excitementBlockY);
            int exciteScale = payload.value("excitementBlockScale", settings->excitementBlockScale);
            int cols = payload.value("gridColumns", settings->gridColumns);
            int rows = payload.value("gridRows", settings->gridRows);
            bool enableGrid = payload.value("enableGridMenu", settings->enableGridMenu);
            bool prevPortraits = settings->enablePortraits;
            bool enablePortraits = payload.value("enablePortraits", settings->enablePortraits);
            settings->SaveLayout(mainX, mainY, mainScale, exciteX, exciteY, exciteScale, cols, rows, enableGrid, enablePortraits);
            if (enablePortraits && !prevPortraits) {
                PortraitManager::GetSingleton()->Initialize();
                UpdateExcitements(true);
            } else if (!enablePortraits && prevPortraits) {
                ClearActorPortraits();
            }
        } else {
            SKSE::log::warn("Magelight UI unknown action: {}", action);
        }
    } catch (const std::exception& e) {
        SKSE::log::error("Magelight UI exception handling action JSON: {}", e.what());
    }
}
