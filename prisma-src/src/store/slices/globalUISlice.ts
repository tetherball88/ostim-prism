// src/store/slices/globalUISlice.ts
import { ActiveMenu, FocusedBlock } from '../../types';
import { StoreSlice } from '../types';
import { alignmentFields, alignmentStepMap } from '../../alignmentConfig';

export const createGlobalUISlice: StoreSlice<import('../types').GlobalUISlice> = (set, get) => ({
    activeMenu: 'navigation',
    focusBlock: 'menu',
    navigatingAt: 0,
    gameReady: false,
    updateActiveMenu: (menu: ActiveMenu) => set((state) => {
        const prevMenu = state.activeMenu;
        console.log(`Updating active menu from ${prevMenu} to ${menu}`);
        if(prevMenu === 'alignMenu' || prevMenu === 'searchMenu') {
            window.sendAction?.(JSON.stringify({
                action: 'focus',
                payload: { shouldFocus: false }
            }));
        }
        if(menu === 'alignMenu' || menu === 'searchMenu') {
            window.sendAction?.(JSON.stringify({
                action: 'focus',
                payload: { shouldFocus: true }
            }));
        }
        console.log(`Focus block after menu update: ${state.focusBlock}`);
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
                if (lowerCaseControl === 'up') state.moveNavigationFocus(1);
                else if (lowerCaseControl === 'down') state.moveNavigationFocus(-1);
                else if (lowerCaseControl === 'left') state.setFocusBlock('buttons');
                else if (lowerCaseControl === 'yes') state.selectNavigationOption();
            } else if (activeMenu === 'utilityOptions') {
                if (lowerCaseControl === 'up') state.moveUtilityOptionsFocus(1);
                else if (lowerCaseControl === 'down') state.moveUtilityOptionsFocus(-1);
                else if (lowerCaseControl === 'left') state.setFocusBlock('buttons');
                else if (lowerCaseControl === 'yes') state.selectUtilityOption();
            }
        }
    },
    setGameReady: () => set(() => ({ gameReady: true })),
});
