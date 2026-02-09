import { create } from 'zustand'
import { ActorStats, NavigationOption } from './types'

const mockDataOptions: NavigationOption[] = Array.from({ length: 20 }, (_, i) => ({
  id: (i + 1).toString(),
  label: `Action Item ${i + 1}`,
  iconPath: "OStim/icons/OStim/positional/squatting_f.dds",
  description: `Action Item ${i + 1}`,
  destination: `/action/${i + 1}`,
}));

const mockDataActors: ActorStats[] = [
    { name: 'Actor 11111111', excitementProgress: 32, gender: 'male', staminaProgress: 50, additionalProgress: 25 },
    { name: 'Actor 2', excitementProgress: 75, gender: 'female', staminaProgress: 60, additionalProgress: -1 },
    { name: 'Actor 3', excitementProgress: 30, gender: 'neither', staminaProgress: 40, additionalProgress: -1 },
]

interface StoreState {
    navigation: {
        options: NavigationOption[]
        activeIndex: number
    }
    actorsState: ActorStats[]
    updateNavigationOptions: (options: NavigationOption[]) => void
    updateActorsState: (actorsState: ActorStats[]) => void
    setActiveIndex: (index: number) => void
    moveFocus: (direction: number) => void
    selectOption: (index?: number) => void
}

export const useOStimStore = create<StoreState>((set, get) => ({
  navigation: {
    options: mockDataOptions,
    activeIndex: 0,
  },
  actorsState: mockDataActors,
  updateNavigationOptions: (options: NavigationOption[]) => set((state) => ({
    navigation: { ...state.navigation, options: [...options], activeIndex: 0 }
  })),
  updateActorsState: (actorsState: ActorStats[]) => set(() => ({ actorsState })),
  setActiveIndex: (index: number) => set((state) => ({
    navigation: { ...state.navigation, activeIndex: index }
  })),
  moveFocus: (direction: number) => set((state) => {
    const { options, activeIndex } = state.navigation;
    const next = Math.max(0, Math.min(activeIndex + direction, options.length - 1));
    return { navigation: { ...state.navigation, activeIndex: next } };
  }),
  selectOption: (index?: number) => {
    const state = get();
    const idx = index ?? state.navigation.activeIndex;
    const option = state.navigation.options[idx];
    if (option) {
      console.log('Selected option:', option.description);
      (window as any).sendAction(JSON.stringify({
        action: "navigationSelect",
        payload: { destination: option.destination }
      }));
    }
  },
}));

// Global control handler - completely outside React lifecycle
(window as any).handleControl = (control: string) => {
  console.log('handleControl', control);
  const store = useOStimStore.getState();
  if (control === 'UP') store.moveFocus(1);
  else if (control === 'DOWN') store.moveFocus(-1);
  else if (control === 'YES') store.selectOption();
};