import { ProgressBar } from "./ProgressBar";
import { ThreadStatus } from "../ThreadStatus/ThreadStatus";

import "./ProgressBarContainer.styles.css"
import { useOStimStore } from "../../store";



export const ProgressBarContainer = () => {
    const actorsState = useOStimStore(state => state.actorsState);

    return (
        <div className="progress-bar-container">
          <div style={{ marginRight: '80px'}}>
            <ThreadStatus />
          </div>
          {actorsState.map((actor, index) => (
              <ProgressBar
                key={actor.name}
                index={index}
              />
          ))}
        </div>
    )
}