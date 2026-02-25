import './Tooltip.styles.css';

interface TooltipProps {
    visible: boolean;
    x: number;
    y: number;
    text: string;
}

export function Tooltip({ visible, x, y, text }: TooltipProps) {
    if (!visible || !text) return null;

    return (
        <div
            className="prisma-tooltip"
            style={{ left: x + 12, top: y - 28 }}
        >
            {text}
        </div>
    );
}
