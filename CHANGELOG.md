# Changelog

## v2.0.1

### Added

- **Play Directly with a Controller or Mouse (No Focus Mode Required)** — You can now browse and control scenes right away using the controller, mouse, or keyboard buttons configured in your OStim MCM, without needing to switch into Focus / Cursor Mode first. Also works seamlessly with controller remapping mods like **Complete Controller Setup** and **Gamepad++**.
- **Right-Stick Scrolling & Arrow Key Support in Focus Mode** — When Focus Mode is active, you can now scroll long lists and icon grids with the controller's Right Stick, toggle the gamepad cursor by clicking the Right Stick to drag or resize panels, and navigate with standard keyboard **Arrow Keys** and **Enter** in addition to your OStim hotkeys.

### Fixed

- **Controller & Mouse Hotkeys in `OStimPrism.ini`** — Fixed `ToggleFocus` and `ToggleInspector` ignoring controller and mouse button codes (such as setting `ToggleFocus = 279` for the controller Y button).
- **Exiting Menus & Focus Mode Cleanly** — Fixed getting stuck in Focus Mode or inside the Search and Alignment menus; pressing **B** on a controller or **Esc** on a keyboard now cleanly backs out of submenus and exits Focus Mode, and the Alignment menu's **Close** button now properly closes the menu.
- **HUD Auto-Hide Wakeup** — Using OStim's Pull Out, Auto-Mode, or FreeCam controls now counts as activity so the HUD wakes back up and resets its idle timer.
- **Tween Menu (`Tab`), Skills, Level-Up & MessageBox Menus** — Added `TweenMenu`, `StatsMenu`, `LevelUp Menu`, and `MessageBoxMenu` to `TrackedMenus` so OStim Prism cleanly hides while they are open instead of overlapping and blocking input.

---

## v2.0.0

### Added

- **Icon Grid Menu Layout** — Navigation and utility menus can now be displayed as a compact icon grid with a bottom label bar, or switched back to the classic vertical list layout at any time.
- **Drag-and-Drop Positioning & Scaling** — Hovering over the main menu panel or the excitement circles panel reveals a layout toolbar to drag each block anywhere on screen, adjust its scale (30%–250%), and set custom grid dimensions (columns × rows). All layout choices are saved automatically to `OStimPrism.ini` so they carry over between saves and sessions.
- **Quick Toggles in the HUD** — Added a **Grid / List** toggle to the main panel's layout toolbar and a **Portraits / No Portraits** toggle to the excitement circles' toolbar, saving your preferences directly to `OStimPrism.ini`.
- **3D Actor Portraits** — When **Phantasm** is installed, excitement circles can now display 3D portraits of each participant with their climax count shown below. If Phantasm is not installed or portraits are toggled off, the circles automatically display the classic climax icon and counter.
- **Gamepad Support** — Added native controller navigation support when the UI is focused.
- **Seamless Focus Switching with Other Overlays** — Clicking between OStim Prism and other Magelight UI mods (such as **OStim Navigator**) now smoothly switches keyboard focus back and forth without closing either overlay.

### Changed

- **Migrated from PrismaUI to Magelight UI** — OStim Prism now runs on Magelight UI for smoother rendering, better performance when hidden, and cleaner keyboard/mouse focus handling.

---

## v1.0.0

### Changed

- Updated for Skyrim Special Edition **1.7.104**.

---

## v0.5.0

### Added

- **Keyboard Input Blocker** — Prevents other mods' hotkeys from triggering while you are typing or focused inside OStim Prism's inputs.

### Changed

- **Improved Mouse Scrolling in List Menus** — Hovering over a list item no longer auto-scrolls the entire list; use the mouse wheel to scroll when browsing with the mouse.

### Fixed

- Fixed an issue where navigating list menus with hotkeys could snap and scroll all the way to the top or bottom.

---

## v0.4.1

- Initial release on Nexus Mods.
