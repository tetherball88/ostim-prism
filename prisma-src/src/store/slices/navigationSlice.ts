// src/store/slices/navigationSlice.ts
import { ListOption } from '../../types';
import { StoreSlice } from '../types';

const mockNavigationOptions: ListOption[] = Array.from({ length: 20 }, (_, i) => ({
  id: (i + 1).toString(),
  iconPath: "",
  description: `Action Item ${i + 1}`,
  destination: `/action/${i + 1}`,
}));

export const createNavigationSlice: StoreSlice<import('../types').NavigationSlice> = (set, get) => ({
    navigation: {
        options: mockNavigationOptions,
        activeIndex: 0,
    },
    updateNavigationOptions: (options: ListOption[]) => set((state) => ({
        navigation: { ...state.navigation, options: [...options], activeIndex: 0 }
    })),
    setNavigationActiveIndex: (index: number) => set((state) => ({
        navigation: { ...state.navigation, activeIndex: index }
    })),
    selectNavigationOption: (index?: number) => {
        const state = get();
        const idx = index ?? state.navigation.activeIndex;
        const option = state.navigation.options[idx];

        console.log('Selected navigation option:', option.description);
        window.sendAction?.(JSON.stringify({
            action: "navigationSelect",
            payload: { destination: option.destination }
        }));
    },
    moveNavigationFocus: (direction: number) => set((state) => {
        const { options, activeIndex } = state.navigation;
        if (options.length === 0) return {};
        const next = Math.max(0, Math.min(activeIndex + direction, options.length - 1));
        return {
            navigatingAt: Date.now(),
            navigation: { ...state.navigation, activeIndex: next }
        };
    }),
    moveNavigationGridFocus: (direction) => {
        const state = get();
        const { options, activeIndex } = state.navigation;
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
                navigation: { ...s.navigation, activeIndex: activeIndex - 1 }
            }));
            return true;
        }

        let next = activeIndex;
        if (direction === 'right') {
            if (activeIndex + 1 < total) {
                next = activeIndex + 1;
            }
        } else if (direction === 'up') {
            // Row 0 is anchored at the bottom; moving visually up advances logicalRow + 1
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
                navigation: { ...s.navigation, activeIndex: next }
            }));
        }
        return true;
    },
});
