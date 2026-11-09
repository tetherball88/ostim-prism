import { FC, useCallback, useEffect, useRef, useState } from "react";
import "./List.styles.css"
import { useOStimStore } from "../../store";
import { Icon } from "../Icon/Icon";
import { ListOption } from "../../types";

const HOVER_SUPPRESS_MS = 150;

// Base values in em units (relative to container font-size)
const ITEM_HEIGHT_EM = 4;     // 4em = 64px at 16px base
const GRID_CELL_EM = 4.25;    // 4em icon + 0.125em (2px) internal padding on each side
const GRID_GAP_EM = 0;        // 0 gap so hitboxes are 100% contiguous with no dead zones between items
const GRID_PAD_EM = 0.15;     // compact outer padding around grid cells
const MAX_VISIBLE_GRID_ROWS = 4;

interface ListProps {
  options: ListOption[];
  activeIndex: number;
  setListActiveIndex: (index: number) => void;
  selectOption: (index: number) => void;
}

export const List: FC<ListProps> = ({ options, activeIndex, setListActiveIndex, selectOption }) => {
  const enableGridMenu = useOStimStore(state => state.settings.enableGridMenu);
  const gridColumns = useOStimStore(state => state.settings.gridColumns);
  const gridRows = useOStimStore(state => state.settings.gridRows);
  const [visibleCount, setVisibleCount] = useState(5); // will be calculated
  const [fontSizePx, setFontSizePx] = useState(16);
  const [hoverIndex, setHoverIndex] = useState<number | null>(null);
  const windowRef = useRef<HTMLDivElement | null>(null);

  // Compute actual pixel height based on container's font-size
  const getFontSize = () => {
    if (!windowRef.current) return fontSizePx;
    return parseFloat(getComputedStyle(windowRef.current).fontSize) || 16;
  };

  const getItemHeight = () => {
    return ITEM_HEIGHT_EM * getFontSize();
  };

  // Calculate visible count from container height
  const updateVisibleCount = useCallback(() => {
    if (!windowRef.current) return;
    const fs = parseFloat(getComputedStyle(windowRef.current).fontSize) || 16;
    setFontSizePx(fs);
    const containerHeight = windowRef.current.clientHeight;
    const itemHeight = ITEM_HEIGHT_EM * fs;
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
  }, [updateVisibleCount, enableGridMenu, gridColumns, gridRows, options.length]);

  useEffect(() => {
    setHoverIndex(null);
  }, [activeIndex]);

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
  const renderedIndex = Math.max(0, Math.min(options.length - 1, hoverIndex ?? activeIndex));

  if (enableGridMenu) {
    const cols = Math.max(1, gridColumns || 4);
    const maxVisibleRows = Math.max(1, gridRows || MAX_VISIBLE_GRID_ROWS);
    const totalRows = Math.max(1, Math.ceil(options.length / cols));
    const visibleRows = Math.min(totalRows, maxVisibleRows);

    const rowStrideEm = GRID_CELL_EM + GRID_GAP_EM;
    const hasOverflow = totalRows > maxVisibleRows;
    // Show ~half of the next hidden row so it's immediately obvious the grid is scrollable
    const peekEm = hasOverflow ? (GRID_CELL_EM * 0.55) : 0;

    // Active row scrolling (row 0 at bottom, row totalRows-1 at top)
    const activeLogicalRow = Math.floor(activeIndex / cols);
    const activeVisualRow = totalRows - 1 - activeLogicalRow;
    const centerRow = Math.floor((visibleRows - 1) / 2);
    const maxRowOffset = Math.max(0, totalRows - visibleRows);
    const idealRowOffset = activeVisualRow - centerRow;
    const rowOffset = Math.max(0, Math.min(idealRowOffset, maxRowOffset));

    const isTopVisible = rowOffset === 0;
    const isBottomVisible = rowOffset >= maxRowOffset;

    // Distribute the peek space: at bottom, peek is at the top edge; at top, peek is at the bottom edge;
    // in the middle, split peek between top and bottom edges while keeping windowHeightEm constant.
    const topShiftEm = !hasOverflow
      ? 0
      : isBottomVisible
        ? peekEm
        : isTopVisible
          ? 0
          : peekEm * 0.5;

    const gridTransformYEm = -rowOffset * rowStrideEm + topShiftEm;

    // 2D selector coordinates for renderedIndex (hover or active) in em units
    const selLogicalRow = Math.floor(renderedIndex / cols);
    const selVisualRow = totalRows - 1 - selLogicalRow;
    const selCol = renderedIndex % cols;
    const selectorXEm = GRID_PAD_EM + selCol * rowStrideEm;
    const selectorYEm = GRID_PAD_EM + (selVisualRow - rowOffset) * rowStrideEm + topShiftEm;

    const baseWindowHeightEm = visibleRows * GRID_CELL_EM + Math.max(0, visibleRows - 1) * GRID_GAP_EM + 2 * GRID_PAD_EM;
    const windowHeightEm = baseWindowHeightEm + peekEm;
    const activeOption = options[renderedIndex];

    return (
      <div className="grid-menu">
        {options.length > 0 && (
          <>
            <div
              className={`grid-window ${isTopVisible ? 'top' : ''} ${isBottomVisible ? 'bottom' : ''}`}
              style={{ height: `${windowHeightEm}em` }}
              ref={windowRef}
              onMouseLeave={() => setHoverIndex(null)}
            >
              <div
                className="grid-selector"
                style={{ transform: `translate(${selectorXEm}em, ${selectorYEm}em)` }}
              />
              <div
                className="grid-container"
                role="listbox"
                style={{
                  gridTemplateColumns: `repeat(${cols}, ${GRID_CELL_EM}em)`,
                  gridTemplateRows: `repeat(${totalRows}, ${GRID_CELL_EM}em)`,
                  gap: `${GRID_GAP_EM}em`,
                  padding: `${GRID_PAD_EM}em`,
                  transform: `translateY(${gridTransformYEm}em)`,
                }}
              >
                {options.map((opt, idx) => {
                  const logicalRow = Math.floor(idx / cols);
                  const visualRow = totalRows - 1 - logicalRow;
                  const col = idx % cols;
                  return (
                    <div
                      key={opt.id + opt.description + opt.iconPath + idx}
                      className={`grid-item ${idx === renderedIndex ? 'active' : ''}`}
                      style={{
                        gridRowStart: visualRow + 1,
                        gridColumnStart: col + 1,
                      }}
                      onMouseEnter={() => {
                        if (Date.now() - useOStimStore.getState().navigatingAt < HOVER_SUPPRESS_MS) return;
                        setHoverIndex(idx);
                      }}
                      onClick={() => {
                        setListActiveIndex(idx);
                        selectOption(idx);
                      }}
                    >
                      <Icon base64Data={opt.iconData} path={opt.iconPath} size="var(--control-buttons-size)" />
                    </div>
                  );
                })}
              </div>
            </div>

            {/* Option A — Left-Accent Pedestal bottom item name bar */}
            <div className="grid-pedestal">
              <p className="grid-pedestal-label">{activeOption?.description || ''}</p>
              <span className="grid-pedestal-count">
                {String(renderedIndex + 1).padStart(2, '0')} / {String(options.length).padStart(2, '0')}
              </span>
            </div>
          </>
        )}
      </div>
    );
  }

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
                className={`list-item ${originalIndex === renderedIndex ? 'active' : ''}`}
                onMouseEnter={() => {
                  if (Date.now() - useOStimStore.getState().navigatingAt < HOVER_SUPPRESS_MS) return;
                  setHoverIndex(originalIndex);
                }}
                onMouseLeave={() => {
                  setHoverIndex(null);
                }}
                onClick={() => {
                  setListActiveIndex(originalIndex);
                  selectOption(originalIndex);
                }}
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
