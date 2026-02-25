export interface ActorStats {
    name: string;
    excitementProgress: number;
    gender: 'male' | 'female' | 'neither';
    staminaProgress: number;
    additionalProgress?: number;
    timesClimaxed: number;
}

export interface ListOption {
    id: string;
    iconPath: string;
    iconData?: string;
    description: string;
    destination: string;
}

export interface ControlButtons {
    id: ActiveMenu;
    iconPath: string;
    iconData?: string;
    description: string;
}

export interface SearchResult {
    sceneId: string;
    name: string;
    actorCount: number;
}

export interface AlignmentData {
    offsetX: number;
    offsetY: number;
    offsetZ: number;
    scale: number;
    rotation: number;
    sosBend: number;
}

export interface AlignmentPayload {
    actorIndex: number;
    actorCount: number;
    sceneId: string;
    sceneName: string;
    actorName: string;
    actorGender: string;
    data: AlignmentData;
}

export type ActiveMenu = 'navigation' | 'utilityOptions' | 'alignMenu' | 'searchMenu';
export type FocusedBlock = 'menu' | 'buttons';

export interface ThreadStatus {
    autoControl: boolean;
    manualControl: boolean;
    locked: boolean;
    currentSpeed: number;
    minSpeed: number;
    maxSpeed: number;
}

export interface Keys {
  keyUp: number;
  keyDown: number;
  keyLeft: number;
  keyRight: number;
  keyYes: number;
  keyEnd: number;
  keyToggle: number;
  keySearch: number;
  keyAlignment: number;
  keySceneStart: number;
  keyNpcSceneStart: number;
  keySpeedUp: number;
  keySpeedDown: number;
  keyPullOut: number;
  keyAutoMode: number;
  keyFreeCam: number;
  keyHideUI: number;
}

declare global {
    interface Window {
        /** Called by Skyrim to update the navigation scene options. */
        updateNavigation(navigationOptions: ListOption[] | string): void;
        /** Called by Skyrim to update the utility options. */
        updateOptions(utilityOptions: ListOption[] | string): void;
        /** Called by Skyrim to update key bindings. */
        updateKeys(keys: Keys | string): void;
        /** Called by Skyrim to update search results. */
        updateSearchResults(results: SearchResult[] | string): void;
        /** Called by Skyrim to update alignment data. */
        updateAlignment(payload: AlignmentPayload | string): void;
        /** Called by Skyrim to update actor excitement/stamina state. */
        updateExcitements(data: ActorStats[] | string): void;
        /** Called by Skyrim to update thread control state and speed. */
        updateThreadStatus(status: ThreadStatus | string): void;
        /** Called by Skyrim to send a control input (up/down/left/yes). */
        handleControl(control: string): void;
        /** Injected by Skyrim/CEF – sends a JSON action string back to the game. */
        sendAction?: (data: string) => void;
        showMenu(menu: ActiveMenu): void;
        setGameReady?: () => void;
    }
}
