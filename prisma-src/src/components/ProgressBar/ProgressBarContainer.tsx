import { useRef } from "react";
import { ProgressBar } from "./ProgressBar";
import { ThreadStatus } from "../ThreadStatus/ThreadStatus";
import { BlockLayoutControls } from "../BlockLayoutControls/BlockLayoutControls";

import "./ProgressBarContainer.styles.css"
import { useOStimStore } from "../../store";



export const ProgressBarContainer = () => {
    const actorsState = useOStimStore(state => state.actorsState);
    const settings = useOStimStore(state => state.settings);
    const saveLayoutSettings = useOStimStore(state => state.saveLayoutSettings);
    const excitementBlockRef = useRef<HTMLDivElement | null>(null);

    return (
        <div
          ref={excitementBlockRef}
          className="hud-block progress-bar-container"
          style={{
            left: settings.excitementBlockX >= 0 ? `${settings.excitementBlockX}px` : '1vw',
            bottom: settings.excitementBlockY >= 0 ? `${settings.excitementBlockY}px` : '20px',
            transform: `scale(${settings.excitementBlockScale / 100})`,
            transformOrigin: 'bottom left',
          }}
        >
          <div style={{ marginRight: '80px'}}>
            <ThreadStatus />
          </div>
          {actorsState.map((actor, index) => (
              <ProgressBar
                key={actor.formId ?? actor.name}
                index={index}
              />
          ))}
          <BlockLayoutControls
            blockRef={excitementBlockRef}
            scale={settings.excitementBlockScale}
            placement="top-left"
            onChange={({ x, y, scale }, commit) =>
              saveLayoutSettings(
                {
                  ...(x !== undefined ? { excitementBlockX: x } : {}),
                  ...(y !== undefined ? { excitementBlockY: y } : {}),
                  ...(scale !== undefined ? { excitementBlockScale: scale } : {}),
                },
                commit
              )
            }
          />
        </div>
    )
}