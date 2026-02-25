import { StateCreator } from 'zustand';
import { Keys } from '../../types';
import { KeysSlice, StoreState } from '../types';

export const createKeySlice: StateCreator<StoreState, [], [], KeysSlice> = (set) => ({
    keys: {
        keyUp: 104,
        keyDown: 101,
        keyLeft: 100,
        keyRight: 102,
        keyYes: 103,
        keyEnd: 110,
        keyToggle: 32,
        keySearch: 75,
        keyAlignment: 76,
        keySceneStart: 38,
        keyNpcSceneStart: 27,
        keySpeedUp: 107,
        keySpeedDown: 109,
        keyPullOut: 91,
        keyAutoMode: 96,
        keyFreeCam: 111,
        keyHideUI: 106
    },
    updateKeys: (keys: Keys) => set({ keys }),
});
