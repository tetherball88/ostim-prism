// src/store/slices/searchSlice.ts
import { SearchResult } from '../../types';
import { StoreSlice } from '../types';

export const createSearchSlice: StoreSlice<import('../types').SearchSlice> = (set, get) => ({
    search: {
        query: '',
        results: [],
        activeIndex: 0,
    },
    updateSearchQuery: (query: string) => set((state) => ({
        search: { ...state.search, query }
    })),
    updateSearchResults: (results: SearchResult[]) => set((state) => ({
        search: {
            ...state.search,
            results: [...results],
            activeIndex: results.length ? Math.min(state.search.activeIndex, results.length - 1) : 0
        }
    })),
    setSearchActiveIndex: (index: number) => set((state) => ({
        search: { ...state.search, activeIndex: index }
    })),
    selectSearchResult: (index?: number) => {
        const state = get();
        const idx = index ?? state.search.activeIndex;
        const result = state.search.results[idx];
        if (result) {
            window.sendAction?.(JSON.stringify({
                action: "searchSelect",
                payload: { sceneId: result.sceneId }
            }));
        }
    },
    moveSearchFocus: (direction: number) => set((state) => {
        const { activeIndex, results } = state.search;
        const next = Math.max(0, Math.min(activeIndex + direction, results.length - 1));
        return { search: { ...state.search, activeIndex: next } };
    }),
});
