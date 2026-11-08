import { useEffect } from "react";
import { isInGame } from "../utils/isInGame";
import { useOStimStore } from "../store";
import { getNormalizedKeyCode } from "../utils/keys";

export const useControls = () => {
  useEffect(() => {
    const mapKeyCodeToControl = (keyCode: number): string | null => {
      const { keys } = useOStimStore.getState();
      if (keyCode === keys.keyUp) return 'up';
      if (keyCode === keys.keyDown) return 'down';
      if (keyCode === keys.keyLeft) return 'left';
      if (keyCode === keys.keyRight) return 'right';
      if (keyCode === keys.keyYes) return 'yes';
      if (keyCode === keys.keyToggle) return 'toggle';
      if (keyCode === keys.keyEnd) return 'no';
      if (keyCode === 27) return 'esc';
      if (keyCode === 9) return 'tab';
      return null;
    };

    const handleKeyDown = (e: KeyboardEvent) => {
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
      const { keys } = useOStimStore.getState();

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
      }

      const controlStr = mapKeyCodeToControl(keyCode);
      if (controlStr) {
        if (keyCode === 27 || keyCode === 9) {
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
      const controlStr = mapKeyCodeToControl(keyCode);
      if (controlStr) {
        window.handleControlEnd?.();
      }
    };

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault();
      window.handleControl(e.deltaY > 0 ? 'up' : 'down');
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    window.addEventListener('wheel', handleWheel, { passive: false });
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
      window.removeEventListener('wheel', handleWheel);
    };
  }, []);
};
