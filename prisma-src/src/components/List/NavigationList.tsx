import { FC } from "react";
import { List } from "./List";
import { useOStimStore } from "../../store";

export const NavigationList: FC = () => {
    const options = useOStimStore(state => state.navigation.options);
    const activeIndex = useOStimStore(state => state.navigation.activeIndex);
    const setNavigationActiveIndex = useOStimStore(state => state.setNavigationActiveIndex);
    const selectOption = useOStimStore(state => state.selectNavigationOption);

    return (
        <List
            options={options}
            activeIndex={activeIndex}
            setListActiveIndex={setNavigationActiveIndex}
            selectOption={selectOption}
        />
    )
}