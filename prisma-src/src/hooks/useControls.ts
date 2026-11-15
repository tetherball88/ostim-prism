import { useEffect } from "react";
import { isInGame } from "../utils/isInGame";
import { useOStimStore } from "../store";
import { getNormalizedKeyCode } from "../utils/keys";
import { MagelightGamepadEvent } from "../types";

const gamepadControlToSkseKeyCode: Record<string, number> = {
  dpadUp: 266,
  dpadDown: 267,
  dpadLeft: 268,
  dpadRight: 269,
  start: 270,
  back: 271,
  leftThumb: 272,
  rightThumb: 273,
  leftShoulder: 274,
  rightShoulder: 275,
  south: 276,
  east: 277,
  west: 278,
  north: 279,
  leftTrigger: 280,
  rightTrigger: 281,
};

const pressOnlyControls = new Set(['yes', 'esc', 'toggle', 'no', 'tab']);

export const useControls = () => {
  useEffect(() => {
    const mapKeyCodeToControl = (keyCode: number, isSearchInput = false): string | null => {
      const { keys } = useOStimStore.getState();
      if (keyCode === keys.keyUp) return 'up';
      if (keyCode === keys.keyDown) return 'down';
      if (keyCode === keys.keyLeft && (!isSearchInput || keyCode !== 37)) return 'left';
      if (keyCode === keys.keyRight && (!isSearchInput || keyCode !== 39)) return 'right';
      if (keyCode === keys.keyYes) return 'yes';
      if (keyCode === keys.keyToggle) return 'toggle';
      if (keyCode === keys.keyEnd || keyCode === keys.keyPullOut) return 'no';
      if (keyCode === 27) return 'esc';
      if (keyCode === 9) return 'tab';
      if (keyCode === 38) return 'up';
      if (keyCode === 40) return 'down';
      if (keyCode === 13) return 'yes';
      if (!isSearchInput && keyCode === 37) return 'left';
      if (!isSearchInput && keyCode === 39) return 'right';
      return null;
    };

    const handleKeyDown = (e: KeyboardEvent) => {
      if (document.activeElement instanceof HTMLInputElement && document.activeElement.classList.contains('block-scale-input')) {
        return;
      }

      if (!isInGame()) {
        if (e.code === 'Numpad8' || e.code === 'ArrowUp') { e.preventDefault(); window.handleControl('up'); }
        if (e.code === 'Numpad5' || e.code === 'ArrowDown') { e.preventDefault(); window.handleControl('down'); }
        if (e.code === 'Numpad4' || e.code === 'ArrowLeft') { e.preventDefault(); window.handleControl('left'); }
        if (e.code === 'Numpad6' || e.code === 'ArrowRight') { e.preventDefault(); window.handleControl('right'); }
        if (e.code === 'Numpad7' || e.code === 'Enter') { e.preventDefault(); window.handleControl('yes'); }
        if (e.keyCode === 27) { e.preventDefault(); window.handleControl('esc'); }
        if (e.keyCode === 9) { e.preventDefault(); window.handleControl('tab'); }
        return;
      }

      // In-game (Magelight UI mode): keyboard events arrive directly on window
      const keyCode = getNormalizedKeyCode(e);
      const { keys, activeMenu } = useOStimStore.getState();
      const isSearchInput =
        document.activeElement instanceof HTMLInputElement &&
        document.activeElement.classList.contains('search-input');

      if (!e.repeat) {
        if (keys.toggleFocusKeys?.includes(keyCode)) {
          e.preventDefault();
          window.sendAction?.(JSON.stringify({ action: 'toggleFocus' }));
          return;
        }
        if (keys.toggleInspectorKeys?.includes(keyCode)) {
          e.preventDefault();
          window.sendAction?.(JSON.stringify({ action: 'toggleInspector' }));
          return;
        }
        if (keyCode === keys.keyHideUI) {
          e.preventDefault();
          window.sendAction?.(JSON.stringify({ action: 'toggleHideUI' }));
          return;
        }
        if (!isSearchInput) {
          if (keyCode === keys.keySearch) {
            e.preventDefault();
            window.showMenu('searchMenu');
            return;
          }
          if (keyCode === keys.keyAlignment) {
            e.preventDefault();
            window.showMenu('alignMenu');
            return;
          }
        }
      }

      const controlStr = mapKeyCodeToControl(keyCode, isSearchInput);
      if (controlStr) {
        if (
          keyCode === 27 ||
          keyCode === 9 ||
          (activeMenu === 'alignMenu' && (controlStr === 'up' || controlStr === 'down'))
        ) {
          e.preventDefault();
        }
        if (!e.repeat) {
          window.handleControlStart?.(controlStr);
        }
      }
    };

    const handleKeyUp = (e: KeyboardEvent) => {
      if (!isInGame()) return;
      const keyCode = getNormalizedKeyCode(e);
      const isSearchInput =
        document.activeElement instanceof HTMLInputElement &&
        document.activeElement.classList.contains('search-input');
      const controlStr = mapKeyCodeToControl(keyCode, isSearchInput);
      if (controlStr) {
        window.handleControlEnd?.();
      }
    };

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault();
      const { focusBlock, activeMenu, settings } = useOStimStore.getState();
      if (focusBlock === 'menu' && (activeMenu === 'navigation' || activeMenu === 'utilityOptions') && settings.enableGridMenu) {
        window.handleControl(e.deltaY > 0 ? 'wheelup' : 'wheeldown');
      } else {
        window.handleControl(e.deltaY > 0 ? 'up' : 'down');
      }
    };

    let rightStickScrollAcc = 0;
    const disposeGamepad = window.magelight?.gamepad?.onAction((ev: MagelightGamepadEvent) => {
      if (ev.phase === 'cancel') {
        rightStickScrollAcc = 0;
        window.handleControlEnd?.();
        return true;
      }
      if (ev.phase === 'release') {
        window.handleControlEnd?.();
        return true;
      }
      if (ev.control === 'rightStick' && ev.phase === 'change') {
        if (Math.abs(ev.y) < 0.25) {
          rightStickScrollAcc = 0;
          return true;
        }
        rightStickScrollAcc += ev.y * (ev.dt || 0.016) * 10;
        while (Math.abs(rightStickScrollAcc) >= 1) {
          const step = rightStickScrollAcc > 0 ? 1 : -1;
          rightStickScrollAcc -= step;
          const { focusBlock, activeMenu, settings } = useOStimStore.getState();
          if (focusBlock === 'menu' && (activeMenu === 'navigation' || activeMenu === 'utilityOptions') && settings.enableGridMenu) {
            window.handleControl(step > 0 ? 'wheelup' : 'wheeldown');
          } else {
            window.handleControl(step > 0 ? 'up' : 'down');
          }
        }
        return true;
      }
      if (ev.phase === 'press' || ev.phase === 'repeat') {
        const skseCode = gamepadControlToSkseKeyCode[ev.control] ?? 0;
        const { keys } = useOStimStore.getState();

        if (ev.phase === 'press' && skseCode) {
          if (keys.toggleFocusKeys?.includes(skseCode)) {
            window.sendAction?.(JSON.stringify({ action: 'toggleFocus' }));
            return true;
          }
          if (keys.toggleInspectorKeys?.includes(skseCode)) {
            window.sendAction?.(JSON.stringify({ action: 'toggleInspector' }));
            return true;
          }
          if (skseCode === keys.keyHideUI) {
            window.sendAction?.(JSON.stringify({ action: 'toggleHideUI' }));
            return true;
          }
          if (skseCode === keys.keySearch) {
            window.showMenu('searchMenu');
            return true;
          }
          if (skseCode === keys.keyAlignment) {
            window.showMenu('alignMenu');
            return true;
          }
        }

        let controlStr: string | null = null;
        switch (ev.action) {
          case 'up':        controlStr = 'up'; break;
          case 'down':      controlStr = 'down'; break;
          case 'left':      controlStr = 'left'; break;
          case 'right':     controlStr = 'right'; break;
          case 'accept':    controlStr = 'yes'; break;
          case 'cancel':    controlStr = 'esc'; break;
          case 'secondary': controlStr = 'toggle'; break;
        }

        if (controlStr === null && skseCode) {
          controlStr = mapKeyCodeToControl(skseCode, false);
        }

        if (controlStr) {
          if (ev.phase === 'press' || !pressOnlyControls.has(controlStr)) {
            window.handleControl(controlStr);
          }
          return true;
        }
      }
      return false;
    });

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    window.addEventListener('wheel', handleWheel, { passive: false });
    return () => {
      disposeGamepad?.();
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
      window.removeEventListener('wheel', handleWheel);
    };
  }, []);
};
