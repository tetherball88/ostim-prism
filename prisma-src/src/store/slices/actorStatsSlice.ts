// src/store/slices/actorStatsSlice.ts
import { ActorStats } from '../../types';
import { StoreSlice } from '../types';

const mockDataActors: ActorStats[] = [
    { formId: 1, name: 'Actor 11111111', excitementProgress: 32, gender: 'male', staminaProgress: 50, additionalProgress: 25, timesClimaxed: 1 },
    { formId: 2, name: 'Actor 2', excitementProgress: 75, gender: 'female', staminaProgress: 60, additionalProgress: -1, timesClimaxed: 2 },
    { formId: 3, name: 'Actor 3', excitementProgress: 30, gender: 'neither', staminaProgress: 40, additionalProgress: -1, timesClimaxed: 0 },
]

export const createActorStatsSlice: StoreSlice<import('../types').ActorStatsSlice> = (set) => ({
    actorsState: mockDataActors,
    updateActorsState: (actorsState: ActorStats[]) => set(() => ({ actorsState })),
    orgasmPinnedUntil: [],
    pinActorOrgasm: (index: number) =>
        set((state) => {
            const updated = [...state.orgasmPinnedUntil];
            updated[index] = Date.now() + 3000;
            return { orgasmPinnedUntil: updated };
        }),
    actorPortraits: {},
    updateActorPortrait: (formId: number, dataUrl: string) =>
        set((state) => ({
            actorPortraits: {
                ...state.actorPortraits,
                [formId]: dataUrl,
            },
        })),
    clearActorPortraits: () => set(() => ({ actorPortraits: {} })),
});
