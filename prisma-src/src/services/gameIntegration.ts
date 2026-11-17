import { useOStimStore } from '../store';
import { StoreState } from '../store/types';
import { ActiveMenu, AlignmentPayload, SearchResult, ThreadStatus, UISettings } from '../types';
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
                const val = parsed[key];
                if (Array.isArray(val)) {
                    convertedKeys[key] = val.map((code: number) => convertScanCodeToJS(code));
                } else {
                    convertedKeys[key] = convertScanCodeToJS(val);
                }
            }
        }
        
        console.log("updateKeys converted:", convertedKeys);
        store.updateKeys(convertedKeys);
    };

    window.updateSettings = (settings: string | UISettings) => {
        const parsed = typeof settings === 'string'
            ? JSON.parse(settings)
            : settings;
        console.log("updateSettings called with:", parsed);
        store.updateSettings(parsed);
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

    window.updateActorPortrait = (formId: number, dataUrl: string) => {
        store.updateActorPortrait(formId, dataUrl);
    };

    window.clearActorPortraits = () => {
        store.clearActorPortraits();
    };

    window.onActorOrgasm = (actorIndex: number) => {
        store.pinActorOrgasm(actorIndex);
    };

    window.updateThreadStatus = (status: ThreadStatus | string) => {
        const parsed = typeof status === 'string' ? JSON.parse(status) : status;
        store.updateThreadStatus(parsed);
    };

    window.showMenu = (menu: ActiveMenu) => {
        const recentStore = useOStimStore.getState()
        const currentMenu = recentStore.activeMenu;
        if(currentMenu == menu) {
            store.updateActiveMenu('navigation');
        } else {
            store.updateActiveMenu(menu);
        }
    };

    const SAME_CONTROL_DEBOUNCE_MS = 150;
    const CROSS_DIR_DEBOUNCE_MS = 60;
    const HOLD_INITIAL_DELAY_MS = 450;
    const HOLD_REPEAT_INTERVAL_MS = 115;

    const lastExecutedAt: Record<string, number> = {};
    let lastAnyDirectionalDiscreteAt = -Infinity;

    window.handleControl = (control: string, isRepeat = false): boolean => {
        const lower = control.toLowerCase();
        if (lower === 'wheelup' || lower === 'wheeldown') {
            store.handleControlInput(control);
            return true;
        }

        const now = performance.now();
        const isDirectional = lower === 'up' || lower === 'down' || lower === 'left' || lower === 'right';
        const isDebouncedAction =
            isDirectional ||
            lower === 'yes' ||
            lower === 'no' ||
            lower === 'esc' ||
            lower === 'toggle' ||
            lower === 'tab';

        if (!isRepeat && isDebouncedAction) {
            const prevSame = lastExecutedAt[lower] ?? -Infinity;
            if (now - prevSame < SAME_CONTROL_DEBOUNCE_MS) {
                return false;
            }
            if (isDirectional && (now - lastAnyDirectionalDiscreteAt < CROSS_DIR_DEBOUNCE_MS)) {
                return false;
            }
        }

        lastExecutedAt[lower] = now;
        if (!isRepeat && isDirectional) {
            lastAnyDirectionalDiscreteAt = now;
        }
        store.handleControlInput(control);
        return true;
    };

    let repeatTimer: ReturnType<typeof setTimeout> | null = null;
    let repeatInterval: ReturnType<typeof setInterval> | null = null;
    let activeHoldControl: string | null = null;

    const stopRepeat = () => {
        if (repeatTimer) {
            clearTimeout(repeatTimer);
            repeatTimer = null;
        }
        if (repeatInterval) {
            clearInterval(repeatInterval);
            repeatInterval = null;
        }
        activeHoldControl = null;
    };

    window.handleControlStart = (control: string) => {
        const recentStore = useOStimStore.getState()
        const lowerCaseControl = control.toLowerCase();
        let shouldListenToHold = false;
        if(recentStore.focusBlock === 'buttons') {
            shouldListenToHold = lowerCaseControl === 'up' || lowerCaseControl === 'down';
        } else if(recentStore.focusBlock === 'menu') {
            if(recentStore.activeMenu === 'alignMenu') {
                shouldListenToHold = lowerCaseControl === 'left' || lowerCaseControl === 'right';
            }

            if(recentStore.activeMenu === 'searchMenu') {
                shouldListenToHold = lowerCaseControl === 'up' || lowerCaseControl === 'down';
            }

            if(recentStore.activeMenu === 'navigation' || recentStore.activeMenu === 'utilityOptions') {
                if (recentStore.settings.enableGridMenu) {
                    shouldListenToHold = lowerCaseControl === 'up' || lowerCaseControl === 'down' || lowerCaseControl === 'left' || lowerCaseControl === 'right';
                } else {
                    shouldListenToHold = lowerCaseControl === 'up' || lowerCaseControl === 'down';
                }
            }
        }
        if (shouldListenToHold) {
            // Ignore duplicate key-start events while the same hold is already active.
            if (activeHoldControl === lowerCaseControl) {
                return;
            }

            stopRepeat();
            const initialFocusBlock = recentStore.focusBlock;
            const executed = window.handleControl(control, false);
            if (!executed) {
                return;
            }
            activeHoldControl = lowerCaseControl;
            if (useOStimStore.getState().focusBlock !== initialFocusBlock) {
                activeHoldControl = null;
                return;
            }
            repeatTimer = setTimeout(() => {
                repeatInterval = setInterval(() => {
                    if (activeHoldControl !== lowerCaseControl) {
                        return;
                    }
                    const prevFocusBlock = useOStimStore.getState().focusBlock;
                    window.handleControl(control, true);
                    if (useOStimStore.getState().focusBlock !== prevFocusBlock) {
                        stopRepeat();
                    }
                }, HOLD_REPEAT_INTERVAL_MS);
            }, HOLD_INITIAL_DELAY_MS);
        } else {
            stopRepeat();
            window.handleControl(control, false);
        }
    };

    window.handleControlEnd = () => {
        stopRepeat();
    };

    // Defensive cleanup in case key-up is missed while focus/visibility changes.
    window.addEventListener('blur', stopRepeat);
    document.addEventListener('visibilitychange', () => {
        if (document.hidden) {
            stopRepeat();
        }
    });

    window.setGameReady = () => {
        console.log("Game signaled it's ready");
        store.setGameReady();
    }
}
