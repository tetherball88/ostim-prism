import { FC, useCallback, useEffect, useRef, useState } from "react";
import "./List.styles.css"
import { useOStimStore } from "../../store";
import { Icon } from "../Icon/Icon";
import { ListOption } from "../../types";

const HOVER_SUPPRESS_MS = 150;

// Base values in em units (relative to container font-size)
const ITEM_HEIGHT_EM = 4;   // 4em = 64px at 16px base

interface ListProps {
  options: ListOption[];
  activeIndex: number;
  setListActiveIndex: (index: number) => void;
  selectOption: (index: number) => void;
}

export const List: FC<ListProps> = ({ options, activeIndex, setListActiveIndex, selectOption }) => {
  const [visibleCount, setVisibleCount] = useState(5); // will be calculated
  const windowRef = useRef<HTMLDivElement | null>(null);

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
      {options.length > 0 && (
        <div className="list-count">
          {activeIndex + 1} / {options.length}
        </div>
      )}
      <div className={`list ${containerTransform === 0 ? 'top' : ''} ${containerTransform === -(getItemHeight() * (options.length - visibleCount)) ? 'bottom' : ''}`} ref={windowRef}>
        {/* Selector fixed within visible window */}
        <div
          className="list-selector"
          style={{ transform: `translateY(${selectorPosition}px)` }}
        />

        <div className="list-container" role="listbox" style={{ transform: `translateY(${containerTransform}px)` }}>
          {reversedOptions.map((opt, visualIdx) => {
            const originalIndex = options.length - 1 - visualIdx;
            return (
              <div
                key={opt.id + opt.description + opt.iconPath}
                className={`list-item ${originalIndex === activeIndex ? 'active' : ''}`}
                onMouseEnter={() => {
                  if (Date.now() - useOStimStore.getState().navigatingAt < HOVER_SUPPRESS_MS) return;
                  setListActiveIndex(originalIndex);
                }}
                onClick={() => selectOption(originalIndex)}
              >
                <Icon base64Data={opt.iconData} path={opt.iconPath} size="var(--control-buttons-size)" />
                <p className="list-label">{opt.description}</p>
              </div>
            );
          })}
        </div>
      </div>
    </>
  );
};
