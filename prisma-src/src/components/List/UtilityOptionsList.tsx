import { FC } from "react";
import { List } from "./List";
import { useOStimStore } from "../../store";

export const UtilityOptionsList: FC = () => {
    const options = useOStimStore(state => state.utilityOptions.options);
    const activeIndex = useOStimStore(state => state.utilityOptions.activeIndex);
    const setUtilityOptionsActiveIndex = useOStimStore(state => state.setUtilityOptionsActiveIndex);
    const selectOption = useOStimStore(state => state.selectUtilityOption);

    return (
        <List
            options={options}
            activeIndex={activeIndex}
            setListActiveIndex={setUtilityOptionsActiveIndex}
            selectOption={selectOption}
        />
    )
}