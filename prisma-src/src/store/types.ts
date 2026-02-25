import { StateCreator } from 'zustand';
import { ActiveMenu, ActorStats, ControlButtons, FocusedBlock, ListOption, SearchResult, AlignmentData, AlignmentPayload, ThreadStatus, Keys } from '../types';

export interface NavigationSlice {
    navigation: {
        options: ListOption[];
        activeIndex: number;
    };
    updateNavigationOptions: (options: ListOption[]) => void;
    setNavigationActiveIndex: (index: number) => void;
    selectNavigationOption: (index?: number) => void;
    moveNavigationFocus: (direction: number) => void;
}

export interface UtilityOptionsSlice {
    utilityOptions: {
        options: ListOption[];
        activeIndex: number;
    };
    updateUtilityOptions: (options: ListOption[]) => void;
    setUtilityOptionsActiveIndex: (index: number) => void;
    selectUtilityOption: (index?: number) => void;
    moveUtilityOptionsFocus: (direction: number) => void;
}

export interface ControlButtonsSlice {
    controlButtons: {
        buttons: ControlButtons[];
        activeIndex: number;
    };
    setButtonsActiveIndex: (index: number) => void;
    moveControlButtonsFocus: (direction: number) => void;
}

export interface SearchSlice {
    search: {
        query: string;
        results: SearchResult[];
        activeIndex: number;
    };
    updateSearchQuery: (query: string) => void;
    updateSearchResults: (results: SearchResult[]) => void;
    setSearchActiveIndex: (index: number) => void;
    selectSearchResult: (index?: number) => void;
    moveSearchFocus: (direction: number) => void;
}

export interface UpdateAlignmentFieldOptions {
    index: number;
    value: number;
    type: 'actorIndex' | 'alignmentData';
}

export interface AlignmentSlice {
    alignment: {
        actorIndex: number;
        actorCount: number;
        sceneId: string;
        sceneName: string;
        actorName: string;
        actorGender: string;
        data: AlignmentData;
        activeField: number;
        inputValues: string[];
    };
    updateAlignment: (payload: AlignmentPayload) => void;
    moveAlignmentField: (direction: number) => void;
    setAlignmentActiveField: (index: number) => void;
    updateAlignmentField: (options: UpdateAlignmentFieldOptions) => void;
}

export interface ActorStatsSlice {
    actorsState: ActorStats[];
    updateActorsState: (actorsState: ActorStats[]) => void;
}

export interface ThreadStatusSlice {
    threadStatus: ThreadStatus;
    updateThreadStatus: (status: ThreadStatus) => void;
    setThreadSpeed: (speed: number) => void;
}

export interface GlobalUISlice {
    activeMenu: ActiveMenu;
    focusBlock: FocusedBlock;
    navigatingAt: number;
    gameReady: boolean;
    updateActiveMenu: (menu: ActiveMenu) => void;
    setFocusBlock: (block: FocusedBlock) => void;
    handleControlInput: (control: string) => void;
    setGameReady: () => void;
}

export interface KeysSlice {
    keys: Keys;
    updateKeys: (keys: Keys) => void;
}

export type StoreState = NavigationSlice & ControlButtonsSlice & SearchSlice & AlignmentSlice & ActorStatsSlice & ThreadStatusSlice & GlobalUISlice & UtilityOptionsSlice & KeysSlice;

export type StoreSlice<T> = StateCreator<StoreState, [], [], T>;
