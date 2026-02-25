import { StoreState } from '../store/types';
import { ActiveMenu, AlignmentPayload, SearchResult, ThreadStatus } from '../types';
import { convertScanCodeToJS } from '../utils/scanCodeMap';

export function setupGameIntegration(store: StoreState) {
    if (!store) {
        console.error("Store not initialized in game integration setup");
        return;
    }

    // Register synchronous callbacks
    window.updateNavigation = (navigationOptions: string | any[]) => {
        console.log("updateNavigation called with:", navigationOptions);
        const options = typeof navigationOptions === 'string'
            ? JSON.parse(navigationOptions)
            : navigationOptions;
        store.updateNavigationOptions(options);
    };

    window.updateOptions = (utilityOptions: string | any[]) => {
        console.log("updateOptions called with:", utilityOptions);
        const options = typeof utilityOptions === 'string'
            ? JSON.parse(utilityOptions)
            : utilityOptions;
        store.updateUtilityOptions(options);
    };

    window.updateKeys = (keys: string | any) => {
        const parsed = typeof keys === 'string'
            ? JSON.parse(keys)
            : keys;
        
        const convertedKeys: any = {};
        // Convert scan codes to JS key codes
        for (const key in parsed) {
            if (Object.prototype.hasOwnProperty.call(parsed, key)) {
                // We assume all values in Keys are numbers (scan codes)
                convertedKeys[key] = convertScanCodeToJS(parsed[key]);
            }
        }
        
        console.log("updateKeys converted:", convertedKeys);
        store.updateKeys(convertedKeys);
    };

    window.updateSearchResults = (results: string | SearchResult[]) => {
        const parsed = typeof results === 'string'
            ? JSON.parse(results)
            : results;
        store.updateSearchResults(parsed);
    };

    window.updateAlignment = (payload: AlignmentPayload | string) => {
        const parsed = typeof payload === 'string'
            ? JSON.parse(payload)
            : payload;
        console.log("updateAlignment called with:", parsed);
        store.updateAlignment(parsed);
    };

    window.updateExcitements = (data: string | any[]) => {
        const actorsState = typeof data === 'string' ? JSON.parse(data) : data;
        store.updateActorsState(actorsState);
    };

    window.updateThreadStatus = (status: ThreadStatus | string) => {
        const parsed = typeof status === 'string' ? JSON.parse(status) : status;
        store.updateThreadStatus(parsed);
    };

    window.showMenu = (menu: ActiveMenu) => {
        console.log("showMenu called with:", menu);
        store.updateActiveMenu(menu);
    };

    window.handleControl = (control: string) => {
        store.handleControlInput(control);
    };

    window.setGameReady = () => {
        console.log("Game signaled it's ready");
        store.setGameReady();
    }
}
