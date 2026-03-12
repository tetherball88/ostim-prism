import { useEffect } from "react";
import { isInGame } from "../utils/isInGame";

export const useControls = () => {
    // Handle keyboard/wheel inputs (for browser testing only)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      console.log('Key down event:', e);
      if (!isInGame()) {
          if (e.code === 'Numpad8' || e.code === 'ArrowUp') { e.preventDefault(); window.handleControl('up'); }
          if (e.code === 'Numpad5' || e.code === 'ArrowDown') { e.preventDefault(); window.handleControl('down'); }
          if (e.code === 'Numpad4' || e.code === 'ArrowLeft') { e.preventDefault(); window.handleControl('left'); }
          if (e.code === 'Numpad6' || e.code === 'ArrowRight') { e.preventDefault(); window.handleControl('right'); }
          if (e.code === 'Numpad7' || e.code === 'Enter') { e.preventDefault(); window.handleControl('yes'); }   
          if (e.keyCode === 27) { e.preventDefault(); window.handleControl('esc'); }
          if (e.keyCode === 9) { e.preventDefault(); window.handleControl('tab'); }   
      }
      
    };

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault();
      window.handleControl(e.deltaY > 0 ? 'up' : 'down');
    };
    
    window.addEventListener('keydown', handleKeyDown);    
    window.addEventListener('wheel', handleWheel, { passive: false });
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('wheel', handleWheel);
    };
  }, []);
}