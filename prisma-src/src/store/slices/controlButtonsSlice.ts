// src/store/slices/controlButtonsSlice.ts
import { StoreSlice } from '../types';

export const createControlButtonsSlice: StoreSlice<import('../types').ControlButtonsSlice> = (set) => ({
    controlButtons: {
        buttons: [
            { id: 'navigation', iconPath: "./assets/icons/OStim/icons/OStim/symbols/rotate_cw.png", description: 'Navigation' },
            { id: 'utilityOptions', iconPath: "./assets/icons/OStim/icons/OStim/symbols/settings.png", description: 'Utility Options' },
            { id: 'alignMenu', iconPath: "./assets/icons/OStim/icons/OStim/symbols/alignment.png", description: 'Align Menu' },
            { id: 'searchMenu', iconPath: "./assets/icons/OStim/icons/OStim/symbols/search.png", description: 'Search' },
        ],
        activeIndex: 0,
    },
    setButtonsActiveIndex: (index: number) => set((state) => ({
        controlButtons: { ...state.controlButtons, activeIndex: index }
    })),
    moveControlButtonsFocus: (direction: number) => set((state) => {
        const { activeIndex, buttons } = state.controlButtons;
        const next = Math.max(0, Math.min(activeIndex + direction, buttons.length - 1));
        return { controlButtons: { ...state.controlButtons, activeIndex: next } };
    }),
});
