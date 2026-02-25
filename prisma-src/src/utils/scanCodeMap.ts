
// Mapping from Skyrim/DirectX Scan Codes to JavaScript Key Codes
// Source for Scan Codes: https://www.creationkit.com/index.php?title=Input_Script
export const scanCodeToJS: Record<number, number> = {
    // Numpad
    71: 103, // Numpad 7
    72: 104, // Numpad 8
    73: 105, // Numpad 9
    75: 100, // Numpad 4
    76: 101, // Numpad 5
    77: 102, // Numpad 6
    79: 97,  // Numpad 1
    80: 98,  // Numpad 2
    81: 99,  // Numpad 3
    82: 96,  // Numpad 0
    83: 110, // Numpad Decimal
    156: 13, // Numpad Enter

    // Arrow Keys (Note: DX often reuses scan codes with extended flags, but OStim usually uses specific ones)
    200: 38, // Up Arrow
    208: 40, // Down Arrow
    203: 37, // Left Arrow
    205: 39, // Right Arrow

    // Common Control Keys
    28: 13,  // Enter
    1: 27,   // Esc
    57: 32,  // Space
    15: 9,   // Tab
    42: 16,  // Left Shift
    54: 16,  // Right Shift
    29: 17,  // Left Ctrl
    157: 17, // Right Ctrl
    56: 18,  // Left Alt
    184: 18, // Right Alt
    14: 8,   // Backspace

    // Alphanumeric
    16: 81, // Q
    17: 87, // W
    18: 69, // E
    19: 82, // R
    20: 84, // T
    21: 89, // Y
    22: 85, // U
    23: 73, // I
    24: 79, // O
    25: 80, // P
    30: 65, // A
    31: 83, // S
    32: 68, // D
    33: 70, // F
    34: 71, // G
    35: 72, // H
    36: 74, // J
    37: 75, // K
    38: 76, // L
    44: 90, // Z
    45: 88, // X
    46: 67, // C
    47: 86, // V
    48: 66, // B
    49: 78, // N
    50: 77, // M
    
    // Numbers
    2: 49, // 1
    3: 50, // 2
    4: 51, // 3
    5: 52, // 4
    6: 53, // 5
    7: 54, // 6
    8: 55, // 7
    9: 56, // 8
    10: 57, // 9
    11: 48, // 0
    
    // Function Keys
    59: 112, // F1
    60: 113, // F2
    61: 114, // F3
    62: 115, // F4
    63: 116, // F5
    64: 117, // F6
    65: 118, // F7
    66: 119, // F8
    67: 120, // F9
    68: 121, // F10
    87: 122, // F11
    88: 123, // F12
    
    // Misc
    181: 111, // Numpad divide? (often mapped to / in games)
    55: 106, // Numpad Multiply (*) - DX 55 is typically * on numpad
    74: 109, // Numpad Subtract (-)
    78: 107, // Numpad Add (+)
};

export const convertScanCodeToJS = (scanCode: number): number => {
    return scanCodeToJS[scanCode] || scanCode;
};
