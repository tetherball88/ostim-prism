import { useCallback, useEffect, useRef, useState } from "react";
import "./Navigation.styles.css"
import { NavigationIcon } from "./NavigationIcon";
import { useOStimStore } from "../../store";

// Base values in em units (relative to container font-size)
const ITEM_HEIGHT_EM = 4;   // 4em = 64px at 16px base

export const Navigation = () => {
  const options = useOStimStore(state => state.navigation.options);
  const activeIndex = useOStimStore(state => state.navigation.activeIndex);
  const setActiveIndex = useOStimStore(state => state.setActiveIndex);
  const moveFocus = useOStimStore(state => state.moveFocus);
  const selectOption = useOStimStore(state => state.selectOption);

  const [visibleCount, setVisibleCount] = useState(5); // will be calculated
  const windowRef = useRef<HTMLDivElement | null>(null);
  const isNavigatingRef = useRef(false);
  const navTimeoutRef = useRef<number | null>(null);

  // Temporarily ignore hover when navigating via keyboard/wheel
  const setNavigating = () => {
    isNavigatingRef.current = true;
    if (navTimeoutRef.current) clearTimeout(navTimeoutRef.current);
    navTimeoutRef.current = window.setTimeout(() => {
      isNavigatingRef.current = false;
    }, 150);
  };

  // Compute actual pixel height based on container's font-size
  const getItemHeight = () => {
    if (!windowRef.current) return 64; // fallback
    const fontSize = parseFloat(getComputedStyle(windowRef.current).fontSize);
    return ITEM_HEIGHT_EM * fontSize;
  };

  // Calculate visible count from container height
  const updateVisibleCount = useCallback(() => {
    if (!windowRef.current) return;
    const containerHeight = windowRef.current.clientHeight;
    const itemHeight = getItemHeight();
    const count = Math.floor(containerHeight / itemHeight);
    setVisibleCount(Math.max(1, count));
  }, []);

  // Update on mount and resize
  useEffect(() => {
    updateVisibleCount();
    const observer = new ResizeObserver(updateVisibleCount);
    if (windowRef.current) {
      observer.observe(windowRef.current);
    }
    return () => observer.disconnect();
  }, [updateVisibleCount]);

  const centerPosition = Math.floor(visibleCount / 2);

  // Reversed items for bottom-to-top display (first item at bottom)
  const reversedOptions = [...options].reverse();

  // Convert activeIndex to visual position in reversed list
  const getVisualIndex = (idx: number) => options.length - 1 - idx;

  // Handle keyboard/wheel inputs (for browser testing)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === 'ArrowUp') { e.preventDefault(); setNavigating(); moveFocus(1); }
      if (e.key === 'ArrowDown') { e.preventDefault(); setNavigating(); moveFocus(-1); }
      if (e.key === 'Enter' || e.key === ' ') selectOption(activeIndex);
    };

    const handleWheel = (e: WheelEvent) => {
      e.preventDefault();
      setNavigating();
      moveFocus(e.deltaY > 0 ? -1 : 1);
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('wheel', handleWheel, { passive: false });
    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('wheel', handleWheel);
    };
  }, [activeIndex, moveFocus, selectOption]);

  // Calculate center-focused scrolling positions
  const getScrollPositions = () => {
    const itemHeight = getItemHeight();
    const visualIndex = getVisualIndex(activeIndex);
    const totalItems = options.length;

    // When fewer items than visible count, push items to bottom
    const bottomOffset = totalItems < visibleCount ? (visibleCount - totalItems) : 0;

    // Calculate container offset to center the active item
    // offset = how many items to shift the container up
    // We want: visualIndex - offset = centerPosition (when possible)
    // So: offset = visualIndex - centerPosition
    // Clamped to: [0, totalItems - visibleCount]
    const maxOffset = Math.max(0, totalItems - visibleCount);
    const idealOffset = visualIndex - centerPosition;
    const containerOffset = Math.max(0, Math.min(idealOffset, maxOffset));

    // Selector position within the visible window
    // When container is offset, selector shows at: visualIndex - containerOffset
    // Add bottomOffset to account for fewer items than container height
    const selectorPosition = visualIndex - containerOffset + bottomOffset;

    return {
      containerTransform: (-containerOffset + bottomOffset) * itemHeight,
      selectorPosition: selectorPosition * itemHeight
    };
  };

  const { containerTransform, selectorPosition } = getScrollPositions();

  return (
    <>
      <div className="skyrim-window" ref={windowRef}>
        {/* Selector fixed within visible window */}
        <div
          className="skyrim-selector"
          style={{ transform: `translateY(${selectorPosition}px)` }}
        />

        <div className="skyrim-items-container" role="listbox" style={{ transform: `translateY(${containerTransform}px)` }}>
          {reversedOptions.map((opt, visualIdx) => {
            const originalIndex = options.length - 1 - visualIdx;
            return (
              <div
                key={opt.id}
                className={`skyrim-item ${originalIndex === activeIndex ? 'active' : ''}`}
                onMouseEnter={() => !isNavigatingRef.current && setActiveIndex(originalIndex)}
                onClick={() => selectOption(originalIndex)}
              >
                <NavigationIcon base64Data={opt.iconData} size="4em" />
                <p className="skyrim-label">{opt.description}</p>
              </div>
            );
          })}
        </div>
      </div>
    </>
  );
};
