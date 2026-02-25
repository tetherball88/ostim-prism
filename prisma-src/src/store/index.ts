// src/store/index.ts
import { create } from 'zustand';
import { devtools } from 'zustand/middleware';
import { StoreState } from './types';
import { createNavigationSlice } from './slices/navigationSlice';
import { createControlButtonsSlice } from './slices/controlButtonsSlice';
import { createSearchSlice } from './slices/searchSlice';
import { createAlignmentSlice } from './slices/alignmentSlice';
import { createActorStatsSlice } from './slices/actorStatsSlice';
import { createGlobalUISlice } from './slices/globalUISlice';
import { createUtilityOptionsSlice } from './slices/utilityOptionsSlice';
import { createThreadStatusSlice } from './slices/threadStatusSlice';
import { createKeySlice } from './slices/keySlice';

export const useOStimStore = create<StoreState>()(
    devtools((...a) => ({
        ...createNavigationSlice(...a),
        ...createControlButtonsSlice(...a),
        ...createSearchSlice(...a),
        ...createAlignmentSlice(...a),
        ...createActorStatsSlice(...a),
        ...createThreadStatusSlice(...a),
        ...createGlobalUISlice(...a),
        ...createUtilityOptionsSlice(...a),
        ...createKeySlice(...a),
    }))
);

export type { StoreState };
