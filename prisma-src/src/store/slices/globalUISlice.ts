// src/store/slices/globalUISlice.ts
import { ActiveMenu, FocusedBlock, UISettings } from '../../types';
import { StoreSlice } from '../types';
import { alignmentFields, alignmentStepMap } from '../../alignmentConfig';
import { isInGame } from '../../utils/isInGame';

const DEV_STORAGE_KEY = 'ostim_prism_layout_settings';

function getInitialSettings(): UISettings {
    const defaults: UISettings = {
        enableGridMenu: true,
        gridColumns: 4,
        gridRows: 4,
        mainBlockX: -1,
        mainBlockY: -1,
        mainBlockScale: 100,
        excitementBlockX: -1,
        excitementBlockY: -1,
        excitementBlockScale: 100,
    };
    if (!isInGame()) {
        try {
            const raw = localStorage.getItem(DEV_STORAGE_KEY);
            if (raw) {
                return { ...defaults, ...JSON.parse(raw) };
            }
        } catch {
            // ignore storage errors
        }
    }
    return defaults;
}

export const createGlobalUISlice: StoreSlice<import('../types').GlobalUISlice> = (set, get) => ({
    activeMenu: 'navigation',
    focusBlock: 'menu',
    navigatingAt: 0,
    gameReady: false,
    settings: getInitialSettings(),
    updateSettings: (newSettings) => set((state) => {
        const merged: UISettings = {
            ...state.settings,
            ...newSettings,
            gridColumns: Math.max(1, Math.min(12, Number(newSettings.gridColumns ?? state.settings.gridColumns) || 4)),
            gridRows: Math.max(1, Math.min(12, Number(newSettings.gridRows ?? state.settings.gridRows) || 4)),
            mainBlockScale: Math.max(30, Math.min(250, Number(newSettings.mainBlockScale ?? state.settings.mainBlockScale) || 100)),
            excitementBlockScale: Math.max(30, Math.min(250, Number(newSettings.excitementBlockScale ?? state.settings.excitementBlockScale) || 100)),
        };
        return { settings: merged };
    }),
    saveLayoutSettings: (newSettings, commit = true) => {
        get().updateSettings(newSettings);
        if (commit) {
            const { settings } = get();
            const payload = {
                mainBlockX: settings.mainBlockX,
                mainBlockY: settings.mainBlockY,
                mainBlockScale: settings.mainBlockScale,
                excitementBlockX: settings.excitementBlockX,
                excitementBlockY: settings.excitementBlockY,
                excitementBlockScale: settings.excitementBlockScale,
                gridColumns: settings.gridColumns,
                gridRows: settings.gridRows,
            };
            window.sendAction?.(JSON.stringify({
                action: 'saveLayout',
                payload,
            }));
            if (!isInGame()) {
                try {
                    localStorage.setItem(DEV_STORAGE_KEY, JSON.stringify(settings));
                } catch {
                    // ignore storage errors
                }
            }
        }
    },
    updateActiveMenu: (menu: ActiveMenu) => set((state) => {
        const prevMenu = state.activeMenu;
        console.log(`Updating active menu from ${prevMenu} to ${menu}`);
        const wasFocusedMenu = prevMenu === 'alignMenu' || prevMenu === 'searchMenu';
        const isFocusedMenu = menu === 'alignMenu' || menu === 'searchMenu';
        if (wasFocusedMenu && !isFocusedMenu) {
            window.sendAction?.(JSON.stringify({
                action: 'focus',
                payload: { shouldFocus: false }
            }));
        } else if (!wasFocusedMenu && isFocusedMenu) {
            window.sendAction?.(JSON.stringify({
                action: 'focus',
                payload: { shouldFocus: true }
            }));
        }
        return {
            activeMenu: menu,
            focusBlock: 'menu',
            controlButtons: { ...state.controlButtons, activeIndex: state.controlButtons.buttons.findIndex(b => b.id === menu) },
        };
    }),
    setFocusBlock: (block: FocusedBlock) => set(() => ({ focusBlock: block })),
    handleControlInput: (control: string) => {
        const lowerCaseControl = control.toLowerCase();
        const state = get();
        const { activeMenu, focusBlock } = state;

        console.log(`Handling control input: ${control} (lowercased: ${lowerCaseControl}), activeMenu: ${activeMenu}, focusBlock: ${focusBlock}`);

        if (focusBlock === 'buttons') {
            if (lowerCaseControl === 'up') state.moveControlButtonsFocus(1);
            else if (lowerCaseControl === 'down') state.moveControlButtonsFocus(-1);
            else if (lowerCaseControl === 'yes') state.updateActiveMenu(state.controlButtons.buttons[state.controlButtons.activeIndex]?.id || 'navigation');
            else if (lowerCaseControl === 'right') state.setFocusBlock('menu');
            else if (lowerCaseControl === 'esc' && (activeMenu === 'alignMenu' || activeMenu === 'searchMenu')) {
                state.updateActiveMenu('navigation');
            }
        } else {
            if (activeMenu === 'alignMenu') {
                const activeFieldIdx = state.alignment.activeField;
                const isCloseButton = activeFieldIdx >= alignmentFields.length;

                if (lowerCaseControl === 'toggle') {
                    const count = state.alignment.actorCount > 0 ? state.alignment.actorCount : 1;
                    const actorFieldIdx = alignmentFields.findIndex(f => f.key === 'actor');
                    const nextActor = (state.alignment.actorIndex + 1) % count;
                    state.updateAlignmentField({ index: actorFieldIdx, value: nextActor + 1, type: "actorIndex" });
                } else if (lowerCaseControl === 'up') state.moveAlignmentField(-1);
                else if (lowerCaseControl === 'down') state.moveAlignmentField(1);
                else if (isCloseButton) {
                    if (lowerCaseControl === 'yes' || lowerCaseControl === 'enter') {
                        state.setFocusBlock('buttons');
                        state.setButtonsActiveIndex(state.controlButtons.buttons.findIndex(b => b.id === 'alignMenu'));
                    }
                } else if (lowerCaseControl === 'left' || lowerCaseControl === 'right') {
                    const field = alignmentFields[activeFieldIdx]?.key ?? 'actor';
                    const direction = lowerCaseControl === 'right' ? 1 : -1;
                    if (field === 'actor') {
                        const count = state.alignment.actorCount > 0 ? state.alignment.actorCount : 1;
                        const raw = state.alignment.inputValues[state.alignment.activeField] ?? "";
                        const parsed = Number(raw);
                        const base = Number.isNaN(parsed) ? (state.alignment.actorIndex + 1) : parsed;
                        const nextActor = (Math.round(base) - 1 + direction + count) % count;
                        state.updateAlignmentField({ index: state.alignment.activeField, value: nextActor + 1, type: "actorIndex" });
                    } else {
                        const delta = (alignmentStepMap[field] || 0) * direction;
                        const raw = state.alignment.inputValues[state.alignment.activeField] ?? "";
                        const parsed = Number(raw);
                        const base = Number.isNaN(parsed) ? state.alignment.data[field] : parsed;
                        const nextValue = base + delta;

                        state.updateAlignmentField({ index: state.alignment.activeField, value: nextValue, type: "alignmentData" });
                    }
                } else if(lowerCaseControl === 'esc' || lowerCaseControl === 'tab') {
                    console.log("Closing alignment menu with control input:", control);
                    state.setFocusBlock('buttons');
                    state.setButtonsActiveIndex(state.controlButtons.buttons.findIndex(b => b.id === 'alignMenu'));
                }
            } else if (activeMenu === 'searchMenu') {
                const activeElement = document.activeElement;
                const isInputFocused = activeElement instanceof HTMLInputElement || activeElement instanceof HTMLTextAreaElement;

                if (isInputFocused && ['up', 'down', 'left', 'right'].includes(lowerCaseControl)) {
                    activeElement.blur();
                }

                if (lowerCaseControl === 'up') {
                    state.moveSearchFocus(-1);
                } else if (lowerCaseControl === 'down') state.moveSearchFocus(1);
                else if(lowerCaseControl === 'left' || lowerCaseControl === 'esc' || lowerCaseControl === 'tab') {
                    state.setFocusBlock('buttons');
                    state.setButtonsActiveIndex(state.controlButtons.buttons.findIndex(b => b.id === 'searchMenu'));
                }
                else if (lowerCaseControl === 'yes') state.selectSearchResult();
            } else if (activeMenu === 'navigation') {
                if (state.settings.enableGridMenu) {
                    if (lowerCaseControl === 'up' || lowerCaseControl === 'down' || lowerCaseControl === 'right') {
                        state.moveNavigationGridFocus(lowerCaseControl);
                    } else if (lowerCaseControl === 'left') {
                        const moved = state.moveNavigationGridFocus('left');
                        if (!moved) state.setFocusBlock('buttons');
                    } else if (lowerCaseControl === 'wheelup') {
                        state.moveNavigationFocus(1);
                    } else if (lowerCaseControl === 'wheeldown') {
                        state.moveNavigationFocus(-1);
                    } else if (lowerCaseControl === 'yes') {
                        state.selectNavigationOption();
                    }
                } else {
                    if (lowerCaseControl === 'up' || lowerCaseControl === 'wheelup') state.moveNavigationFocus(1);
                    else if (lowerCaseControl === 'down' || lowerCaseControl === 'wheeldown') state.moveNavigationFocus(-1);
                    else if (lowerCaseControl === 'left') state.setFocusBlock('buttons');
                    else if (lowerCaseControl === 'yes') state.selectNavigationOption();
                }
            } else if (activeMenu === 'utilityOptions') {
                if (state.settings.enableGridMenu) {
                    if (lowerCaseControl === 'up' || lowerCaseControl === 'down' || lowerCaseControl === 'right') {
                        state.moveUtilityOptionsGridFocus(lowerCaseControl);
                    } else if (lowerCaseControl === 'left') {
                        const moved = state.moveUtilityOptionsGridFocus('left');
                        if (!moved) state.setFocusBlock('buttons');
                    } else if (lowerCaseControl === 'wheelup') {
                        state.moveUtilityOptionsFocus(1);
                    } else if (lowerCaseControl === 'wheeldown') {
                        state.moveUtilityOptionsFocus(-1);
                    } else if (lowerCaseControl === 'yes') {
                        state.selectUtilityOption();
                    }
                } else {
                    if (lowerCaseControl === 'up' || lowerCaseControl === 'wheelup') state.moveUtilityOptionsFocus(1);
                    else if (lowerCaseControl === 'down' || lowerCaseControl === 'wheeldown') state.moveUtilityOptionsFocus(-1);
                    else if (lowerCaseControl === 'left') state.setFocusBlock('buttons');
                    else if (lowerCaseControl === 'yes') state.selectUtilityOption();
                }
            }
        }
    },
    setGameReady: () => set(() => ({ gameReady: true })),
});
