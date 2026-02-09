export interface ActorStats {
    name: string;
    excitementProgress: number;
    gender: 'male' | 'female' | 'neither';
    staminaProgress: number;
    additionalProgress?: number;
}

export interface NavigationOption {
    id: string;
    iconPath: string;
    iconData?: string;
    description: string;
    destination: string;
}