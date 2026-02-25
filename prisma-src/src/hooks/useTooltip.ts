import { useState } from 'react';

export interface TooltipState {
    visible: boolean;
    x: number;
    y: number;
    text: string;
}

/** Returns tooltip state + a factory that produces the three mouse handlers for any element. */
export function useTooltip() {
    const [tooltip, setTooltip] = useState<TooltipState>({ visible: false, x: 0, y: 0, text: '' });

    const tooltipOn = (text: string) => ({
        onMouseEnter: (e: React.MouseEvent) => setTooltip({ visible: true, x: e.clientX, y: e.clientY, text }),
        onMouseMove:  (e: React.MouseEvent) => setTooltip(prev => ({ ...prev, x: e.clientX, y: e.clientY })),
        onMouseLeave: () => setTooltip(prev => ({ ...prev, visible: false })),
    });

    return { tooltip, tooltipOn };
}
