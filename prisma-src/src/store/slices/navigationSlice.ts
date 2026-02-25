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
        const next = Math.max(0, Math.min(activeIndex + direction, options.length - 1));
        return { navigation: { ...state.navigation, activeIndex: next } };
    }),
});
