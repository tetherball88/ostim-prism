# Magelight UI Migration Guide (OStim Navigator, M.A.R.A.S, and OCompass)

This guide documents how to migrate **OStim Navigator**, **M.A.R.A.S**, and **OCompass** from **PrismaUI** (`PrismaUI_API.h`) to **Magelight UI** (`MagelightUI_API.h` v4), based on:

- Magelight UI's official migration guide ([`MagelightUI/docs/PRISMAUI_MIGRATION.md`](https://github.com/Severause/MagelightUI/blob/main/docs/PRISMAUI_MIGRATION.md))
- Magelight UI v4 C++ header (`SKSE_Source/src/MagelightUI_API.h`)
- The completed reference migration in **OStim Prism** (commit `b185781`, [`SKSE_Source/src/PrismaUIManager.cpp`](SKSE_Source/src/PrismaUIManager.cpp) and [`prisma-src`](prisma-src))

---

## 1. Key Architectural Shifts (Lessons from OStim Prism & Magelight Docs)

Before touching individual files, align all three mods around five core changes that Magelight UI introduces compared to PrismaUI:

### 1.1 Persistent Hidden View Instead of Destroy / Recreate Churn
All three target mods currently call `Destroy()` in `kPreLoadGame` (and **OStim Navigator** / **OCompass** also call `Destroy()` in `ThreadEnded`, with OStim Navigator even running a detached `2000ms` polling loop waiting for `HasFocus` to clear before destroying the view).

In Magelight UI (1.5+ / Ultralight 2.0):
- Create a **single persistent view** during initialization (`startVisible = false`).
- On `kPreLoadGame` and `ThreadEnded`, call `Hide()` (`ShowView(view, false)` + `CancelUIModeRequest(modId)` + `ReleaseUIMode(modId)`) instead of destroying the view.
- Hidden views pause automatically (`document.visibilityState === 'hidden'`, `requestAnimationFrame`, CSS animations/transitions, and `ResizeObserver` pause), while timers, `InteropCall`, and `InvokeJS` remain active.
- **Important Ultralight 2.0 side effects:**
  - `transitionend` and `animationend` will not fire while a view is hidden; they wait until the next show. Avoid blocking C++ or UI state transitions on CSS transition completion while closing.
  - `resize` and `ResizeObserver` callbacks fire once when the view is shown or prepainted.
  - `SetViewPrepaint(view, true)` (`hostVersionNumber >= 3111`) keeps rendering warm off-screen for instant show, at the cost of extra GPU memory (~15 MB+ at 1440p from Magelight 1.5.0). Use `SetViewLoadOnShow(view, true)` (`>= 3101`) instead for views opened rarely.

### 1.2 Mandatory `PostGameTask` Thread-Safety Contract for JS Listeners
Magelight JS listeners (`RegisterJSListener` / `RegisterJSListenerEx`) run on the **render/page thread mid-frame**. Per `MagelightUI_API.h`:
- **Never** mutate game/plugin state directly inside a JS listener.
- **Never** call `SKSE::GetTaskInterface()->AddTask(...)` directly inside a JS listener (it contends on SKSE's task lock and can deadlock on Skyrim VR).

**Audit finding:** All three target mods violate this today (especially **OCompass**, which invokes `dataProvider` methods synchronously on the JS thread, and **OStim Navigator** / **M.A.R.A.S**, which call `SKSE::GetTaskInterface()->AddTask` directly inside JS listeners).

**Reference pattern (`OStim Prism` `OnAction` / `OnActionGameTask`):**
1. Copy the incoming C-string payload onto the heap (`new std::string(data ? data : "")`) immediately inside the JS listener.
2. If `magelight->hostVersionNumber >= 3001`, call `magelight->PostGameTask(&OnActionGameTask, copy)` (which executes safely on the game thread).
3. Otherwise (host `< 3001`), spawn a detached `std::thread` that posts to `SKSE::GetTaskInterface()->AddTask`, freeing `copy` inside the task.

### 1.3 Delete `KeyboardInputBlocker`, `FocusMenu` Hacks, and `setTextInputFocus` Roundtrips
PrismaUI mods commonly needed manual Skyrim input blockers, `numPausesGame` workarounds, custom FocusMenu polling, or JS->C++ text-input focus toggles (`notifyTextFocus` / `mousedown` -> `refocusView`).

Magelight handles keyboard, text entry, mouse, pause, Escape capture, and gamepad cleanly inside `RequestUIMode` and view setters:
- Use `kUIModeFlagPause` (`1`) when game pause is wanted.
- Use `kUIModeFlagNoTextEntry` (`4`) for menus that do not need typing (important for VR compatibility).
- Use `SetEscapeCapture(view, capture)` (`>= 2800`) to lock/unlock Escape dynamically (e.g., locked during confirmation modals, unlocked on main panels).
- Use `SetViewGamepad(view, &pad)` (`>= 10600`) for controller/gamepad handling.

### 1.4 Clean UI Mode Acquisition, Release, and Local Visibility Tracking
- Magelight does **not** have an `IsHidden` query like PrismaUI—track `m_isViewShown` locally when calling `ShowView` and handling view destruction/shutdown events.
- Subscribe to view/UI-mode events (`CallbackThread::GameThread`) to keep local focus state in sync with external events (e.g., user pressing Escape, another mod reclaiming UI mode, render crash, or host shutdown):
  - `Event::UIModeEntered` (`4`)
  - `Event::UIModeExited` (`5`)
  - `Event::FocusDenied` (`6`)
  - `Event::UIModeRefused` (`11`)
  - `Event::ViewDomReady` (`0`), `Event::ViewLoadFailed` (`1`), `Event::ViewDestroyed` (`3`), `Event::RenderDead` (`9`), `Event::HostShutdown` (`10`)
- Support reclaim/cancel cleanly (`OStim Prism` `SetFocus` pattern):
  - When acquiring focus: include `kUIModeFlagReclaim` (`8`) only if `hostVersionNumber >= 10100 && QueryCapability("uimodecancel") == 1`.
  - When releasing focus: call `CancelUIModeRequest(modId)` (if `>= 10100 && QueryCapability("uimodecancel") == 1`) before `ReleaseUIMode(modId)`.
  - Use `IsUIModeActive()` / `GetUIModeOwner()` when checking cross-mod UI ownership instead of assuming exclusive control.

### 1.5 Ultralight 2.0 Keyboard Events, Muted Hotkeys, Storage, and Network
- **`KeyboardEvent.code` is now populated** (Ultralight 1.x left `e.code` as `""`). Port OStim Prism's `getNormalizedKeyCode(e)` (`prisma-src/src/utils/keys.ts`) so numpad keys map reliably via `e.code` before falling back to `e.keyCode`.
- **Focused-mode hotkeys:** When Magelight UI mode is active, Skyrim's `BSInputDeviceManager` and other mods' `GetAsyncKeyState` polling are muted by default. Any hotkey expected to work while the UI is focused (unfocus, hide panel, toggle inspector) should be handled in JS `window` keyboard listeners -> `InteropCall` -> `PostGameTask` (or explicitly configured via `kUIModeFlagPassPolledKeys` (`16`) on `>= 10800` with `QueryCapability("polledKeys") == 1`).
- **`localStorage` & Network:** `localStorage` is isolated per `modId` (PrismaUI `localStorage` data will not transfer automatically). Network policy defaults to `FileOnly` (`>= 3000`); if remote requests are ever added, configure `SetNetworkPolicy` (`>= 2802`).

---

## 2. Common Migration Steps (All Mods)

### 2.1 Replace the C++ API Header and Update Paths
1. Copy `MagelightUI_API.h` (from `OStim Prism Dev/SKSE_Source/src/MagelightUI_API.h` or Magelight UI SDK) into each mod's `SKSE_Source/src/` (or `include/`) and remove `PrismaUI_API.h`.
2. Update `SKSE_Source/CMakeLists.txt` if `PrismaUI_API.h` is listed explicitly in source/header lists (most globs automatically pick up `MagelightUI_API.h`).
3. Change view asset directory from:
   - `Data/PrismaUI/views/<ViewName>/index.html`
   to:
   - `Data/Magelight/<ModId>/views/<ViewName>/index.html`
4. Resolve the path robustly from the Skyrim executable directory (as done in `OStim Prism` `GetPagePath()`):
   ```cpp
   wchar_t buf[MAX_PATH]{};
   GetModuleFileNameW(nullptr, buf, MAX_PATH);
   auto path = std::filesystem::path(buf).parent_path()
       / L"Data" / L"Magelight" / L"<ModId>" / L"views" / L"<ViewName>" / L"index.html";
   // Convert with .u8string() when passing UTF-8 page path to ViewDesc
   ```

### 2.2 PrismaUI -> Magelight UI v4 API Mapping Table

| Concern | PrismaUI (`PRISMA_UI_API::IVPrismaUI1`) | Magelight UI v4 (`MAGELIGHT_API::MagelightApi4`) |
| --- | --- | --- |
| Acquire API | `PRISMA_UI_API::RequestPluginAPI()` | `MAGELIGHT_API::RequestApi4()` |
| Mod registration | *(none)* | `RegisterMod(&desc, &modId)` (`ModDesc`, `PackVersion(maj, min, pat)`) / `UnregisterMod(modId)` |
| Create view | `CreateView(relPath, onDomReady)` | `CreateViewEx(modId, &vd, &view)` (`ViewDesc`, `startVisible = false`, `Layer`, `Anchor`, event callback on `CallbackThread::GameThread`) |
| Destroy / Reload / URL | `Destroy(view)` | `DestroyView(view)`, `ReloadView(view)`, `Navigate(view, url)` |
| JS -> C++ listener | `RegisterJSListener(view, name, cb)` | `RegisterJSListener(view, name, cb)` / `RegisterJSListenerEx(view, name, cb, user)` **+ marshal via `PostGameTask` (`>= 3001`)** |
| C++ -> JS calls | `InteropCall(view, fn, arg)` / `Invoke(view, script)` | `InteropCall(view, fn, arg)` / `InvokeJS(view, script)` |
| Show / Hide | `Show(view)` / `Hide(view)` / `IsHidden(view)` | `ShowView(view, true/false)` (track `m_isViewShown` locally) |
| Validity | `IsValid(view)` | `IsViewValid(view)` |
| Focus / Pause | `Focus(view, pauseGame)` / `Unfocus(view)` / `HasFocus(view)` | `RequestUIMode(view, flags)` (`kUIModeFlagNone`, `kUIModeFlagPause`, `kUIModeFlagQueue`, `kUIModeFlagNoTextEntry`, `kUIModeFlagReclaim`, `kUIModeFlagPassPolledKeys`), `CancelUIModeRequest(modId)`, `ReleaseUIMode(modId)`, `GetUIModeOwner()`, `IsUIModeActive()` |
| Escape key | Manual menu blocking (`FocusMenu` / hooks) | `SetEscapeCapture(view, capture)` (`hostVersionNumber >= 2800`) |
| Inspector | Create / SetBounds / Show / Destroy | `ShowInspector(view, show)` & `IsInspectorVisible(view, &out)` (`>= 2800 && QueryCapability("inspector") == 1`) |
| Performance / Memory | Pre-destroyed & recreated | Persistent hidden view + `SetViewFreezeWorld` (`>= 3100`), `SetViewLoadOnShow` (`>= 3101`), `SetViewPrepaint` (`>= 3111`) |
| Gamepad | Custom/menu routing | `SetViewGamepad(view, &pad)` (`>= 10600`) |
| Error diagnostics | Logs | `GetLastErrorMessage(modId)` + `Result` enum (`Ok=0`, `HostAbsent=1`, ..., `Denied=9`, ...) |

### 2.3 Reference C++ Patterns from OStim Prism
Copy/adapt these exact helpers from `OStim Prism Dev/SKSE_Source/src/PrismaUIManager.cpp`:
1. **`Initialize()` (`RequestApi4` + `RegisterMod` + `CreateViewEx`):**
   - Check `magelight = MAGELIGHT_API::RequestApi4()`.
   - Register mod with `ModDesc` and `RegisterMod(&desc, &modId)`.
   - Populate `ViewDesc` using `GetPagePath()`, `startVisible = false`, appropriate `Layer` (`Panel` / `Hud` / `Popup`), and event callback with `CallbackThread::GameThread`.
   - Call `CreateViewEx(modId, &vd, &view)`. Log `GetLastErrorMessage(modId)` on non-`Result::Ok`.
   - Configure `SetEscapeCapture`, `SetViewGamepad` (`>= 10600`), and `SetViewPrepaint` (`>= 3111`) / `SetViewLoadOnShow` (`>= 3101`) as needed.
2. **`SetFocus(bool focus)`:**
   - Acquiring: build flags (`kUIModeFlagPause` if needed, `kUIModeFlagNoTextEntry` if no text fields, plus `kUIModeFlagReclaim` when `hostVersionNumber >= 10100 && QueryCapability("uimodecancel") == 1`), then call `RequestUIMode(view, flags)`.
   - Releasing: if `hostVersionNumber >= 10100 && QueryCapability("uimodecancel") == 1`, call `CancelUIModeRequest(modId)`, then `ReleaseUIMode(modId)`.
3. **`ToggleInspector()`:**
   - Gate on `hostVersionNumber >= 2800 && QueryCapability("inspector") == 1`.
   - Call `IsInspectorVisible(view, &visible)` then `ShowInspector(view, !visible)`.
4. **Thread-safe JS Listener (`OnAction` / `OnActionGameTask`):**
   - Copy payload into `new std::string(data)` -> if `hostVersionNumber >= 3001`, call `magelight->PostGameTask(&OnActionGameTask, copy)`, else spawn detached `std::thread` that calls `SKSE::GetTaskInterface()->AddTask` and deletes `copy`.

### 2.4 Common Frontend (`prisma-src`) & Packaging Changes
- **`vite.config.ts`:** All three mods already configure `base: './'` and `remove-module-attrs`. Only update `build.outDir` from `../PrismaUI/views/<ViewName>` to `../Magelight/<ModId>/views/<ViewName>`.
- **Release workflows:** Replace `PrismaUI/` with `Magelight/` in release zip commands so distributed archives ship `Data/Magelight/...`.
- **Keyboard normalization:** Port `getNormalizedKeyCode(e)` from `OStim Prism Dev/prisma-src/src/utils/keys.ts` (`numKeys` map on `e.code` before `e.keyCode`).

---

## 3. Mod-by-Mod Migration Blueprint: OStim Navigator

**Target workspace:** `OStim Navigator Dev`

### 3.1 Files to Modify / Delete
- **Delete:** `SKSE_Source/src/KeyboardInputBlocker.h`, `SKSE_Source/src/KeyboardInputBlocker.cpp`
- **Modify (C++):**
  - `SKSE_Source/CMakeLists.txt` (remove `KeyboardInputBlocker`, update `PrismaUI_API.h` -> `MagelightUI_API.h` if listed)
  - `SKSE_Source/plugin.cpp` (remove `kInputLoaded` registration and input-blocker init)
  - `SKSE_Source/src/UI.cpp` (update focus/visibility calls to match new `PrismaUIManager` API)
  - `SKSE_Source/src/Papyrus.cpp` (verify any UI toggle calls work with persistent hidden view)
  - `SKSE_Source/src/PrismaUIManager.h` & `SKSE_Source/src/PrismaUIManager.cpp`
- **Modify (Frontend & CI):**
  - `prisma-src/vite.config.ts`
  - `prisma-src/src/App.tsx`
  - `prisma-src/src/services/gameIntegration.ts`
  - `.github/workflows/main-release.yml`

### 3.2 C++ Migration Details
1. **Remove Input Blocking & Focus Polling Hacks:**
   - Remove `KeyboardInputBlocker` completely and drop `kInputLoaded` in `plugin.cpp`.
   - Remove `SetGamePaused` `numPausesGame` menu flag hacks; use `kUIModeFlagPause` in `RequestUIMode` whenever Navigator wants the game paused.
   - Remove `CheckTextInputFocus`, `SetTextInputFocus`, and `OnSetTextInputFocus`. Magelight handles DOM `<input>` / `<textarea>` text entry natively inside UI mode.
   - Remove the detached `2000ms` polling loop waiting for `HasFocus` to clear before `Destroy()`.

2. **Switch to Persistent Hidden View:**
   - Replace `CreateView` + `Destroy()` churn in `kPreLoadGame` and `ThreadEnded` with single creation at `Initialize()` (`startVisible = false`, `Layer::Popup`).
   - **Important z-order note:** Modal/popup editors (like `OStimNavigator`) that open over another fullscreen `Layer::Panel` view (such as `OStimPrism`) **must** use `MAGELIGHT_API::Layer::Popup` (`layer 2`) and call `magelight->RaiseView(view)` in `Show()` / `SetFocus(true)`. Otherwise, a fullscreen `clickThrough = false` `Layer::Panel` view created later (like `OStimPrism`) sits on top of it and intercepts all mouse clicks even when the popup has UI mode / keyboard focus.
   - On `kPreLoadGame` and `ThreadEnded`, call `Hide()` (`ShowView(view, false)` + `CancelUIModeRequest` + `ReleaseUIMode`).
   - Track `m_isViewShown` locally and synchronize `m_hasFocus` via `OnMagelightEvent` (`UIModeEntered`, `UIModeExited`, `UIModeRefused`, `FocusDenied`, `ViewDestroyed`, `HostShutdown`).

3. **Fix All JS Listeners Calling `AddTask` Directly:**
   Currently, OStim Navigator calls `SKSE::GetTaskInterface()->AddTask` directly on the JS thread in **11 remaining listeners** (plus `OnSetTextInputFocus`, which should be deleted):
   - `OnSaveSceneJson`
   - `OnRefocusView`
   - `OnWarpToScene`
   - `OnCloseNavigator`
   - `OnGenerateDescription`
   - `OnRequestAutoDescription` (`SKSE_Source/src/PrismaUIManager.cpp#L808-L825`)
   - `OnRequestCustomDescription` (`SKSE_Source/src/PrismaUIManager.cpp#L827-L839`)
   - `OnSaveCustomDescription` (`SKSE_Source/src/PrismaUIManager.cpp#L841-L868`)
   - `OnRequestSceneMeta` (`SKSE_Source/src/PrismaUIManager.cpp#L882-L902`)
   - `OnSaveSceneMeta` (`SKSE_Source/src/PrismaUIManager.cpp#L904-L936`)
   - `OnRequestSceneJson` (`SKSE_Source/src/PrismaUIManager.cpp#L938-L975`)

   Refactor all of them behind a shared helper wrapping `PostGameTask` (`>= 3001`, falling back to detached `std::thread` -> `AddTask`). *(Note: inside `OnRequestSceneJson`, after spawning the background `std::thread` to read the scene file from disk, posting back via `AddTask` from that worker thread is already off the JS render thread—only the initial entry from the JS listener into the game thread must switch to `PostGameTask`.)*

### 3.3 Frontend & Release Workflow Details
- **Offline Monaco Editor (`prisma-src/src/services/monacoSetup.ts` & `src/main.tsx`):**
  - `@monaco-editor/react` loads Monaco from `cdn.jsdelivr.net` by default, which is blocked under Magelight's default `NetworkPolicy::FileOnly` (`>= 0.30.0`).
  - Configure `loader.config({ monaco })` in `monacoSetup.ts` (imported in `main.tsx`) using local `monaco-editor/esm/vs/editor/editor.api`, `monaco-editor/esm/vs/language/json/monaco.contribution` (`monaco.languages.json = jsonContribution`), and Vite inline workers (`editor.worker?worker&inline` and `json.worker?worker&inline` via `self.MonacoEnvironment.getWorker`) so Monaco runs 100% locally with zero network requests.
- **`prisma-src/src/services/gameIntegration.ts` & `App.tsx`:**
  - Remove `notifyTextFocus` and `mousedown` -> `refocusView` workarounds.
  - Because `BSInputDeviceManager` is muted while UI mode is active, keep close/toggle hotkeys working while focused via React `window` `keydown` listeners (using `getNormalizedKeyCode(e)` for numpad support).
- **`prisma-src/vite.config.ts`:** Change `outDir` to `../Magelight/OStimNavigator/views/OStimNavigator` (matching the view path configured in `GetPagePath()`). Keep `base: './'` and `remove-module-attrs`.
- **`.github/workflows/main-release.yml`:** Update line 32 in the `Create selective zip` step from `PrismaUI/` to `Magelight/`.

---

## 4. Mod-by-Mod Migration Blueprint: M.A.R.A.S

**Target workspace:** `M.A.R.A.S Dev`

### 4.1 Files to Modify
- **C++:**
  - `SKSE_Source/CMakeLists.txt`
  - `SKSE_Source/plugin.cpp`
  - `SKSE_Source/include/ui/PrismaUIManager.h` & `SKSE_Source/src/ui/PrismaUIManager.cpp`
  - `SKSE_Source/include/ui/PrismaModalManager.h` & `SKSE_Source/src/ui/PrismaModalManager.cpp`
  - `SKSE_Source/src/ui/ConfirmationModal.cpp`
  - `SKSE_Source/src/ui/PrismaUIDataBridge.cpp`
- **Frontend & CI:**
  - `prisma-src/vite.config.ts`
  - `prisma-src/src/App.tsx`
  - `prisma-src/src/store/prismaBridge.ts`
  - `prisma-src/src/components/modals/PrismaModalHost.tsx`
  - `.github/workflows/main-release.yml`

### 4.2 Eliminating `PrismaUI_FocusMenu` and `m_modalFocusLocked`
M.A.R.A.S currently maintains a complex focus state machine around PrismaUI limitations:
- `MenuOpenCloseEvent` watches `PrismaUI_FocusMenu` opening/closing.
- `SyncViewVisibilityAndFocus` uses `m_modalFocusLocked` to perform an unfocus -> wait -> refocus dance just to change pause behavior or avoid menu conflicts.
- `RunAfterFocusReleased` queues `m_postUnfocusCallbacks` (used by `ConfirmationModal::FireTriggerTextInput`) waiting for `PrismaUI_FocusMenu` to close before opening native Skyrim text input menus.

With Magelight UI v4, replace all of this cleanly:
1. **Remove `PrismaUI_FocusMenu` monitoring** from `MenuOpenCloseEvent`.
2. **Delete `m_modalFocusLocked` and the unfocus -> wait -> refocus dance** in `SyncViewVisibilityAndFocus`. Magelight's `RequestUIMode` updates `kUIModeFlagPause` **in place** (`>= 0.26.11`), so opening/closing a modal can simply call `RequestUIMode(view, updatedFlags)` directly.
3. **Lock Escape on blocking modals cleanly:** Use `magelight->SetEscapeCapture(view, hasModal)` (`hostVersionNumber >= 2800`) when `PrismaModalManager` has an active modal, and pass `false` on the normal dashboard so Escape exits UI mode naturally.
4. **Track `m_isViewShown` locally:** Magelight has no `IsHidden` query; update `m_isViewShown` in `ShowView` wrappers and clear state on `ViewDestroyed` / `RenderDead` / `HostShutdown`.
5. **Flush `m_postUnfocusCallbacks` via `Event::UIModeExited`:** When `OnMagelightEvent` (`CallbackThread::GameThread`) receives `Event::UIModeExited` (and also guard against `UIModeRefused` / `FocusDenied` / `ViewDestroyed` so callers don't stall), drain `m_postUnfocusCallbacks` so `ConfirmationModal::FireTriggerTextInput` triggers reliably right after Magelight finishes releasing Skyrim UI mode.

### 4.3 Lifecycle & Thread-Safety Fixes
- **Persistent Hidden View:** Replace `Destroy()` in `kPreLoadGame` with persistent creation at `Initialize()` (`startVisible = false`) and `Hide()` on `kPreLoadGame`. Consider `SetViewPrepaint` vs `SetViewLoadOnShow` depending on whether the M.A.R.A.S dashboard is opened frequently.
- **JS Listener Thread Safety:** `OnCloseMARAS`, `OnRefocusView`, and `PrismaUIDataBridge::HandleAction` (called synchronously by `OnMarasAction`) currently call `SKSE::GetTaskInterface()->AddTask` directly from the JS thread. Marshal all of them through `PostGameTask` (`>= 3001`, fallback detached `std::thread` -> `AddTask`).

### 4.4 Frontend & Release Workflow Details
- **`prisma-src/src/store/prismaBridge.ts` & `PrismaModalHost.tsx` / `App.tsx`:**
  - Verify Escape key handling works cleanly alongside `SetEscapeCapture(view, hasModal)` (do not rely on closing animations finishing via `transitionend` while hidden).
  - Add `getNormalizedKeyCode(e)` for numpad keys since `KeyboardEvent.code` is now populated.
- **`prisma-src/vite.config.ts`:** Update `outDir` to `../Magelight/MARAS/views/...` (matching `GetPagePath()`), keeping `base: './'` and `remove-module-attrs`.
- **`.github/workflows/main-release.yml`:** Update the zip step from `PrismaUI/` to `Magelight/`.

---

## 5. Mod-by-Mod Migration Blueprint: OCompass

**Target workspace:** `OCompass Dev`

### 5.1 Files to Modify
- **C++:**
  - `SKSE_Source/CMakeLists.txt`
  - `SKSE_Source/plugin.cpp`
  - `SKSE_Source/src/Papyrus.cpp`
  - `SKSE_Source/src/PrismaUIManager.h` & `SKSE_Source/src/PrismaUIManager.cpp`
- **Frontend:**
  - `prisma-src/vite.config.ts`
  - `prisma-src/src/App.tsx`
  - `prisma-src/src/services/gameIntegration.ts`
- *(Note: `OCompass Dev` currently has no `.github/workflows/*.yml`; update any manual or local packaging setup to ship `Data/Magelight/...` instead of `Data/PrismaUI/...`.)*

### 5.2 Critical Thread-Safety Fixes (Highest Priority)
OCompass currently has the most severe JS-thread safety issues of the three mods:
- `OnAction` calls `SKSE::GetTaskInterface()->AddTask` directly on the JS listener thread.
- Multiple JS handlers call `dataProvider->NavigateToSearchResult`, `SwapActors`, `RemoveActorFromThread`, and `AddActorToThread` **directly on the JS listener thread without any task marshalling at all**.

**Required fix:** Refactor every JS listener to immediately serialize/copy its payload and dispatch to the game thread via `PostGameTask` (`>= 3001`, fallback detached `std::thread` -> `AddTask`) **before** touching `dataProvider`, OStim threads, or `SKSE` interfaces.

### 5.3 Persistent View, Inspector, and Focus Sync
1. **Persistent Hidden View:** Replace `Destroy()` calls in `kPreLoadGame` and `ThreadEnded` with single creation at `Initialize()` (`startVisible = false`) and `Hide()` (`ShowView(view, false)` + `CancelUIModeRequest` + `ReleaseUIMode`). Because OCompass is a lightweight HUD/compass overlay used dynamically during gameplay, evaluate `SetViewPrepaint` (`>= 3111`, faster show, ~15 MB+ extra GPU memory at 1440p) vs `SetViewLoadOnShow` (`>= 3101`).
2. **Inspector API:** Remove `inspectorCreated` tracking and bounds calls; replace with `IsInspectorVisible(view, &visible)` + `ShowInspector(view, !visible)` gated on `hostVersionNumber >= 2800 && QueryCapability("inspector") == 1`.
3. **Synchronize `UpdateFocusState()` with Magelight Events:** Subscribe `OnMagelightEvent` (`CallbackThread::GameThread`) to `Event::UIModeEntered`, `Event::UIModeExited`, and `Event::UIModeRefused` (plus `FocusDenied`, `ViewDestroyed`, `HostShutdown`) so `UpdateFocusState()` updates immediately whenever Escape exits UI mode or focus is lost/refused.
4. **VR Text Entry Flag:** Consider passing `kUIModeFlagNoTextEntry` (`4`) in `RequestUIMode` when OCompass is focused without an active search/text input.

### 5.4 Solving Muted Hotkeys While Focused
Because Magelight mutes `BSInputDeviceManager` and other mods' `GetAsyncKeyState` while UI mode is active, OCompass's Skyrim-side `toggleFocusHotkey`, `toggleVisibilityHotkey`, and `toggleInspectorHotkey` will **stop firing once the compass gains focus**.

**Fix:**
- Listen for those hotkeys (or Escape / panel toggle bindings configured from C++ settings) inside `prisma-src` (`App.tsx` / `services/gameIntegration.ts` `window` `keydown` listener) while focused, sending an action back to C++ (`InteropCall` / registered listener -> `PostGameTask`) to unfocus, hide, or toggle the inspector.
- *(Alternative if bare polled keys must pass through to native C++ input: check `hostVersionNumber >= 10800 && QueryCapability("polledKeys") == 1` and pass `kUIModeFlagPassPolledKeys`, though handling focused shortcuts inside JS avoids cross-mod key leakage.)*
- Apply `getNormalizedKeyCode(e)` for reliable numpad/key handling under Ultralight 2.0.

### 5.5 Frontend Output Path
- **`prisma-src/vite.config.ts`:** Update `build.outDir` to `../Magelight/OCompass/views/OCompass` (matching `GetPagePath()`), keeping `base: './'` and `remove-module-attrs`.

---

## 6. Verification Checklist (Per Migrated Mod)

After migrating each mod:
1. **Build Frontend:** Run `npm run build` in `prisma-src/` and confirm bundled files output cleanly under `Data/Magelight/<ModId>/views/<ViewName>/index.html`.
2. **Build SKSE DLL:** Compile `SKSE_Source` and confirm zero references remain to `PrismaUI_API.h`, `KeyboardInputBlocker` (Navigator), or `PrismaUI_FocusMenu` (M.A.R.A.S).
3. **Runtime Log Check:** Launch Skyrim with Magelight UI installed; verify `RegisterMod` and `CreateViewEx` return `MAGELIGHT_API::Result::Ok` (or inspect `GetLastErrorMessage(modId)` on any non-`Ok` result).
4. **Functional Checks:**
   - Open/close view repeatedly, save/load (`kPreLoadGame`), and start/end OStim threads (`ThreadEnded` for Navigator & OCompass) without crashes or stalled `HasFocus` polling.
   - Verify Escape unlocks/exits dashboard views cleanly, and stays locked during M.A.R.A.S confirmation modals until dismissed (`ConfirmationModal::FireTriggerTextInput` fires cleanly after `Event::UIModeExited`).
   - Verify OCompass `toggleFocusHotkey` / `toggleVisibilityHotkey` / `toggleInspectorHotkey` work both unfocused and focused.
   - Verify no game freezes, deadlocks, or Skyrim VR crashes when triggering JS actions rapidly (`PostGameTask` path).
