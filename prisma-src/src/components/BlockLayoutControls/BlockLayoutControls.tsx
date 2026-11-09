import { FC, useEffect, useRef, useState } from 'react';
import './BlockLayoutControls.styles.css';

interface BlockLayoutControlsProps {
  blockRef: React.RefObject<HTMLDivElement | null>;
  scale: number;
  placement?: 'top-left' | 'bottom-left';
  gridColumns?: number;
  gridRows?: number;
  onChange: (
    update: { x?: number; y?: number; scale?: number; gridColumns?: number; gridRows?: number },
    commit: boolean
  ) => void;
}

const MIN_SCALE = 30;
const MAX_SCALE = 250;
const SCALE_STEP = 5;
const MIN_GRID = 1;
const MAX_GRID = 12;

export const BlockLayoutControls: FC<BlockLayoutControlsProps> = ({
  blockRef,
  scale,
  placement = 'top-left',
  gridColumns,
  gridRows,
  onChange,
}) => {
  const [isDragging, setIsDragging] = useState(false);
  const [isEditing, setIsEditing] = useState(false);
  const [inputValue, setInputValue] = useState(`${scale}%`);
  const [isEditingCols, setIsEditingCols] = useState(false);
  const [colsInput, setColsInput] = useState(String(gridColumns ?? 4));
  const [isEditingRows, setIsEditingRows] = useState(false);
  const [rowsInput, setRowsInput] = useState(String(gridRows ?? 4));
  const cancelGridBlurRef = useRef(false);

  useEffect(() => {
    if (!isEditing) {
      setInputValue(`${scale}%`);
    }
  }, [scale, isEditing]);

  useEffect(() => {
    if (!isEditingCols && gridColumns !== undefined) {
      setColsInput(String(gridColumns));
    }
  }, [gridColumns, isEditingCols]);

  useEffect(() => {
    if (!isEditingRows && gridRows !== undefined) {
      setRowsInput(String(gridRows));
    }
  }, [gridRows, isEditingRows]);

  const handleDragMouseDown = (e: React.MouseEvent<HTMLDivElement>) => {
    if (e.button !== 0) return;
    e.preventDefault();
    e.stopPropagation();

    const el = blockRef.current;
    if (!el) return;

    const rect = el.getBoundingClientRect();
    const startClientX = e.clientX;
    const startClientY = e.clientY;
    // Since transform-origin is bottom-left, rect.left and (innerHeight - rect.bottom)
    // are the exact CSS left and bottom coordinates of the block.
    const startX = Math.round(rect.left);
    const startY = Math.round(window.innerHeight - rect.bottom);

    setIsDragging(true);

    const handleMouseMove = (moveEvent: MouseEvent) => {
      const dx = moveEvent.clientX - startClientX;
      const dy = moveEvent.clientY - startClientY;
      const nextX = Math.max(0, Math.min(window.innerWidth - 40, Math.round(startX + dx)));
      const nextY = Math.max(0, Math.min(window.innerHeight - 40, Math.round(startY - dy)));
      onChange({ x: nextX, y: nextY }, false);
    };

    const handleMouseUp = (upEvent: MouseEvent) => {
      window.removeEventListener('mousemove', handleMouseMove);
      window.removeEventListener('mouseup', handleMouseUp);
      setIsDragging(false);

      const dx = upEvent.clientX - startClientX;
      const dy = upEvent.clientY - startClientY;
      const nextX = Math.max(0, Math.min(window.innerWidth - 40, Math.round(startX + dx)));
      const nextY = Math.max(0, Math.min(window.innerHeight - 40, Math.round(startY - dy)));
      onChange({ x: nextX, y: nextY }, true);
    };

    window.addEventListener('mousemove', handleMouseMove);
    window.addEventListener('mouseup', handleMouseUp);
  };

  const commitScaleInput = () => {
    setIsEditing(false);
    const cleaned = inputValue.replace(/[^0-9]/g, '');
    const parsed = parseInt(cleaned, 10);
    if (Number.isNaN(parsed)) {
      setInputValue(`${scale}%`);
      return;
    }
    const clamped = Math.max(MIN_SCALE, Math.min(MAX_SCALE, parsed));
    setInputValue(`${clamped}%`);
    if (clamped !== scale) {
      onChange({ scale: clamped }, true);
    }
  };

  const stepScale = (delta: number) => {
    const next = Math.max(MIN_SCALE, Math.min(MAX_SCALE, scale + delta));
    if (next !== scale) {
      onChange({ scale: next }, true);
    }
  };

  const commitGridInput = (
    raw: string,
    currentVal: number,
    setVal: (val: string) => void,
    setEditing: (editing: boolean) => void,
    field: 'gridColumns' | 'gridRows'
  ) => {
    setEditing(false);
    const cleaned = raw.replace(/[^0-9]/g, '');
    const parsed = parseInt(cleaned, 10);
    if (Number.isNaN(parsed)) {
      setVal(String(currentVal));
      return;
    }
    const clamped = Math.max(MIN_GRID, Math.min(MAX_GRID, parsed));
    setVal(String(clamped));
    if (clamped !== currentVal) {
      onChange({ [field]: clamped }, true);
    }
  };

  const stepGridInput = (
    raw: string,
    delta: number,
    currentVal: number,
    setVal: (val: string) => void,
    field: 'gridColumns' | 'gridRows'
  ) => {
    const parsed = parseInt(raw.replace(/[^0-9]/g, ''), 10);
    const base = Number.isNaN(parsed) ? currentVal : parsed;
    const next = Math.max(MIN_GRID, Math.min(MAX_GRID, base + delta));
    setVal(String(next));
    if (next !== currentVal) {
      onChange({ [field]: next }, true);
    }
  };

  const gridFields =
    gridColumns !== undefined && gridRows !== undefined
      ? [
          {
            field: 'gridColumns' as const,
            label: 'Cols',
            title: 'Grid columns (1-12)',
            value: gridColumns,
            inputValue: colsInput,
            setInputValue: setColsInput,
            setIsEditing: setIsEditingCols,
          },
          {
            field: 'gridRows' as const,
            label: 'Rows',
            title: 'Grid rows (1-12)',
            value: gridRows,
            inputValue: rowsInput,
            setInputValue: setRowsInput,
            setIsEditing: setIsEditingRows,
          },
        ]
      : null;

  // Counter-scale the toolbar so it remains a consistent readable size regardless of block scale
  const counterScale = 100 / Math.max(MIN_SCALE, scale);

  return (
    <div
      className={`block-layout-controls ${placement} ${isDragging || isEditing || isEditingCols || isEditingRows ? 'active' : ''}`}
      style={{
        transform: `scale(${counterScale})`,
        transformOrigin: placement === 'bottom-left' ? 'top left' : 'bottom left',
      }}
      onClick={(e) => e.stopPropagation()}
    >
      <div
        className={`block-drag-handle ${isDragging ? 'dragging' : ''}`}
        onMouseDown={handleDragMouseDown}
        title="Drag to move block"
      >
        <svg viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round">
          <polyline points="5 9 2 12 5 15" />
          <polyline points="9 5 12 2 15 5" />
          <polyline points="15 19 12 22 9 19" />
          <polyline points="19 9 22 12 19 15" />
          <line x1="2" y1="12" x2="22" y2="12" />
          <line x1="12" y1="2" x2="12" y2="22" />
        </svg>
      </div>

      <div className="block-scale-controls">
        <button
          type="button"
          className="block-scale-btn"
          onClick={() => stepScale(-SCALE_STEP)}
          title="Decrease scale"
        >
          −
        </button>
        <input
          type="text"
          className="block-scale-input"
          value={inputValue}
          onFocus={(e) => {
            setIsEditing(true);
            setInputValue(String(scale));
            e.currentTarget.select();
          }}
          onChange={(e) => setInputValue(e.target.value)}
          onBlur={commitScaleInput}
          onKeyDown={(e) => {
            e.stopPropagation();
            if (e.key === 'Enter') {
              e.currentTarget.blur();
            } else if (e.key === 'Escape') {
              setIsEditing(false);
              setInputValue(`${scale}%`);
              e.currentTarget.blur();
            }
          }}
        />
        <button
          type="button"
          className="block-scale-btn"
          onClick={() => stepScale(SCALE_STEP)}
          title="Increase scale"
        >
          +
        </button>
      </div>

      {gridFields && (
        <div className="block-grid-controls">
          {gridFields.map((item, idx) => (
            <span key={item.field} style={{ display: 'inline-flex', alignItems: 'center', gap: '0.3rem' }}>
              {idx > 0 && <span className="block-grid-sep">×</span>}
              <label className="block-grid-field" title={item.title}>
                <span className="block-grid-label">{item.label}</span>
                <input
                  type="text"
                  className="block-scale-input block-grid-input"
                  value={item.inputValue}
                  onFocus={(e) => {
                    cancelGridBlurRef.current = false;
                    item.setIsEditing(true);
                    item.setInputValue(String(item.value));
                    e.currentTarget.select();
                  }}
                  onChange={(e) => item.setInputValue(e.target.value)}
                  onBlur={() => {
                    if (cancelGridBlurRef.current) {
                      cancelGridBlurRef.current = false;
                      return;
                    }
                    commitGridInput(
                      item.inputValue,
                      item.value,
                      item.setInputValue,
                      item.setIsEditing,
                      item.field
                    );
                  }}
                  onKeyDown={(e) => {
                    e.stopPropagation();
                    if (e.key === 'Enter') {
                      e.currentTarget.blur();
                    } else if (e.key === 'Escape') {
                      cancelGridBlurRef.current = true;
                      item.setIsEditing(false);
                      item.setInputValue(String(item.value));
                      e.currentTarget.blur();
                    } else if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
                      e.preventDefault();
                      stepGridInput(
                        item.inputValue,
                        e.key === 'ArrowUp' ? 1 : -1,
                        item.value,
                        item.setInputValue,
                        item.field
                      );
                    }
                  }}
                />
              </label>
            </span>
          ))}
        </div>
      )}
    </div>
  );
};
