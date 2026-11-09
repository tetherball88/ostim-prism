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
        if (options.length === 0) return {};
        const next = Math.max(0, Math.min(activeIndex + direction, options.length - 1));
        return {
            navigatingAt: Date.now(),
            utilityOptions: { ...state.utilityOptions, activeIndex: next }
        };
    }),
    moveUtilityOptionsGridFocus: (direction) => {
        const state = get();
        const { options, activeIndex } = state.utilityOptions;
        const total = options.length;
        if (total === 0) return false;

        const cols = Math.max(1, state.settings.gridColumns || 4);
        const col = activeIndex % cols;
        const logicalRow = Math.floor(activeIndex / cols);
        const totalRows = Math.ceil(total / cols);

        if (direction === 'left') {
            if (col === 0) return false;
            set((s) => ({
                navigatingAt: Date.now(),
                utilityOptions: { ...s.utilityOptions, activeIndex: activeIndex - 1 }
            }));
            return true;
        }

        let next = activeIndex;
        if (direction === 'right') {
            if (activeIndex + 1 < total) {
                next = activeIndex + 1;
            }
        } else if (direction === 'up') {
            if (logicalRow < totalRows - 1) {
                next = Math.min(total - 1, activeIndex + cols);
            }
        } else if (direction === 'down') {
            if (logicalRow > 0) {
                next = activeIndex - cols;
            }
        }

        if (next !== activeIndex) {
            set((s) => ({
                navigatingAt: Date.now(),
                utilityOptions: { ...s.utilityOptions, activeIndex: next }
            }));
        }
        return true;
    },
});
