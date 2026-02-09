import { ProgressBar } from "./ProgressBar";

import "./ProgressBarContainer.styles.css"
import { useOStimStore } from "../../store";



export const ProgressBarContainer = () => {
    const actorsState = useOStimStore(state => state.actorsState);

    return (
        <div className="progress-bar-container">
          {actorsState.map((actor) => (
              <ProgressBar
                key={actor.name}
                {...actor}
              />
          ))}
        </div>
    )
}