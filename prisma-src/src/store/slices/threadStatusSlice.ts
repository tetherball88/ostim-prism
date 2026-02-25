// src/store/slices/threadStatusSlice.ts
import { ThreadStatus } from '../../types';
import { StoreSlice } from '../types';

const initialThreadStatus: ThreadStatus = {
    autoControl: false,
    manualControl: false,
    locked: false,
    currentSpeed: 1,
    minSpeed: 1,
    maxSpeed: 1,
};

export const createThreadStatusSlice: StoreSlice<import('../types').ThreadStatusSlice> = (set, get) => ({
    threadStatus: initialThreadStatus,
    updateThreadStatus: (status: ThreadStatus) => set(() => ({ threadStatus: status })),
    setThreadSpeed: (speed: number) => {
        const { threadStatus } = get();
        const clamped = Math.max(threadStatus.minSpeed, Math.min(threadStatus.maxSpeed, speed));
        set(state => ({ threadStatus: { ...state.threadStatus, currentSpeed: clamped } }));
        window.sendAction?.(JSON.stringify({ action: 'setSpeed', payload: { speed: clamped } }));
    },
});
