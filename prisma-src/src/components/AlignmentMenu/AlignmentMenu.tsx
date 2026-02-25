import { useEffect, useRef } from "react";
import { useOStimStore } from "../../store";
import { isInGame } from "../../utils/isInGame";
import { alignmentFields } from "../../alignmentConfig";
import "./AlignmentMenu.styles.css";

export const AlignmentMenu = () => {
  const alignment = useOStimStore(state => state.alignment);
  const updateAlignment = useOStimStore(state => state.updateAlignment);
  const updateAlignmentField = useOStimStore(state => state.updateAlignmentField);
  const setAlignmentActiveField = useOStimStore(state => state.setAlignmentActiveField);
  const updateActiveMenu = useOStimStore(state => state.updateActiveMenu);
  const setFocusBlock = useOStimStore(state => state.setFocusBlock);
  const setButtonsActiveIndex = useOStimStore(state => state.setButtonsActiveIndex);
  const controlButtons = useOStimStore(state => state.controlButtons);
  const focusBlock = useOStimStore(state => state.focusBlock);
  const keys = useOStimStore(state => state.keys);

  const inputRefs = useRef<Array<HTMLInputElement | null>>([]);
  const inputValues = alignment.inputValues;

  useEffect(() => {
    let lastKey: null | number = null;
    const handleKeyDown = (event: KeyboardEvent) => {
      lastKey = event.keyCode;
    }
    const numKeys: number[] = [keys.keyDown, keys.keyUp, keys.keyLeft, keys.keyRight, keys.keyYes];
    const handleBeforeInput = (event: InputEvent) => {
      if (lastKey !== null && numKeys.some(k => k === lastKey)) {
        event.preventDefault();
      }
    }
    window.addEventListener("keydown", handleKeyDown, { capture: true });
    window.addEventListener("beforeinput", handleBeforeInput, { capture: true });
    return () => {
      window.removeEventListener("keydown", handleKeyDown, { capture: true });
      window.removeEventListener("beforeinput", handleBeforeInput, { capture: true });
    }
  }, [])

  useEffect(() => {
    if (isInGame()) {
      window.sendAction?.(JSON.stringify({
        action: "alignmentInit",
        payload: { actorIndex: alignment.actorIndex }
      }));
    } else {
      updateAlignment({
        actorIndex: 0,
        actorCount: 2,
        sceneId: "DebugScene",
        sceneName: "Debug Scene",
        actorName: "Debug Actor",
        actorGender: "female",
        data: {
          offsetX: 0,
          offsetY: 0,
          offsetZ: 0,
          scale: 1,
          rotation: 0,
          sosBend: 0
        }
      });
    }
  }, []);

  useEffect(() => {
    const target = inputRefs.current[alignment.activeField];
    if (target) {
      if(focusBlock === 'menu') {
        target.focus();
        target.select();
      } else {
        target.blur();
      }
    }
  }, [focusBlock, alignment.activeField, alignment.actorIndex]);

  const handleInputFocus = (event: React.FocusEvent<HTMLInputElement>) => {
    event.target.select();
    if (isInGame()) {
      window.sendAction?.(JSON.stringify({ action: "setTextInputFocus", payload: { focused: true } }));
    }
  };

  const handleInputBlur = () => {
    if (isInGame()) {
      window.sendAction?.(JSON.stringify({ action: "setTextInputFocus", payload: { focused: false } }));
    }
  };

  return (
    <div className="align-panel" aria-label="Alignment menu">
      <div key={alignment.actorName + alignment.actorIndex} className="align-header">
        <span className="align-title">Alignment</span>
        <div className="align-info-container">
          <div className="align-info-row">
            <span className="align-info-label">Scene:</span>
            <span className="align-info-value">{alignment.sceneName || alignment.sceneId}</span>
            <span className="align-info-sub">({alignment.sceneId})</span>
          </div>
          <div className="align-info-row">
            <span className="align-info-label">Actor:</span>
            <span className="align-info-value">{alignment.actorName}</span>
            <span className="align-info-sub">({alignment.actorGender})</span>
            <span className="align-info-index">[{alignment.actorIndex + 1}/{Math.max(alignment.actorCount, 1)}]</span>
          </div>
        </div>
      </div>

      <div className="align-fields" role="listbox" aria-activedescendant={`align-field-${alignment.activeField}`}>
        {alignmentFields.map((field, index) => (
          <div
            key={field.key + alignment.actorIndex}
            id={`align-field-${index}`}
            className={`align-field ${index === alignment.activeField ? "active" : ""}`}
            onClick={() => setAlignmentActiveField(index)}
          >
            <span className="align-label">{field.label}</span>
            <input
              ref={(node) => { inputRefs.current[index] = node; }}
              className="align-input"
              onClick={() => setAlignmentActiveField(index)}
              type="number"
              step={field.step}
              value={inputValues[index] ?? ""}
              onFocus={handleInputFocus}
              onChange={(event) => {
                const parsed = event.target.valueAsNumber;
                updateAlignmentField({
                  index,
                  value: parsed,
                  type: field.key === 'actor' ? 'actorIndex' : 'alignmentData'
                });
              }}
              onBlur={handleInputBlur}
            />
          </div>
        ))}
      </div>

      <div className="align-hint">
        Up/Down: Select • Left/Right: Adjust • Type: Set Value • Tab/Close: Close
      </div>
      
      <button 
        className={`align-close-button ${alignment.activeField === alignmentFields.length ? "active" : ""}`}
        onClick={() => {
          updateActiveMenu('navigation');
          setFocusBlock('buttons');
          setButtonsActiveIndex(controlButtons.buttons.findIndex(b => b.id === 'alignMenu'));
        }}>
        Close
      </button>
    </div>
  );
};
