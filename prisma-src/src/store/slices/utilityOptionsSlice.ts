// src/store/slices/navigationSlice.ts
import { ListOption } from '../../types';
import { StoreSlice } from '../types';

const mockUtilityOptions: ListOption[] = [
  { id: '1', iconPath: "", description: 'Utility Option 1', destination: '/utility/1' },
  { id: '2', iconPath: "", description: 'Utility Option 2', destination: '/utility/2' },
  { id: '3', iconPath: "", description: 'Utility Option 3', destination: '/utility/3' },
];

export const createUtilityOptionsSlice: StoreSlice<import('../types').UtilityOptionsSlice> = (set, get) => ({
    utilityOptions: {
        options: mockUtilityOptions,
        activeIndex: 0,
    },
    updateUtilityOptions: (options: ListOption[]) => set((state) => ({
        utilityOptions: { ...state.utilityOptions, options: [...options], activeIndex: 0 }
    })),
    setUtilityOptionsActiveIndex: (index: number) => set((state) => ({
        utilityOptions: { ...state.utilityOptions, activeIndex: index }
    })),
    selectUtilityOption: (index?: number) => {
        const state = get();
        const idx = index ?? state.utilityOptions.activeIndex;
        const option = state.utilityOptions.options[idx];

        console.log('Selected utility option index:', idx);
        window.sendAction?.(JSON.stringify({
            action: "selectOption",
            payload: { index: parseInt(option.id) }
        }));
    },
    moveUtilityOptionsFocus: (direction: number) => set((state) => {
        const { options, activeIndex } = state.utilityOptions;
        const next = Math.max(0, Math.min(activeIndex + direction, options.length - 1));
        return { utilityOptions: { ...state.utilityOptions, activeIndex: next } };
    }),
});
