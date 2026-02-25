import { useOStimStore } from '../../store';
import { useTooltip } from '../../hooks/useTooltip';
import { Tooltip } from '../Tooltip/Tooltip';
import './ThreadStatus.styles.css';

export function ThreadStatus() {
    const { autoControl, manualControl, locked, currentSpeed, minSpeed, maxSpeed } =
        useOStimStore(state => state.threadStatus);
    const setThreadSpeed = useOStimStore(state => state.setThreadSpeed);

    const { tooltip, tooltipOn } = useTooltip();

    const size = 120;
    const cx = 60;
    const cy = 60;
    const strokeWidth = 10;
    const radius = (size - strokeWidth) / 2; // 55
    const circumference = radius * 2 * Math.PI;
    const rotation = 135; // gap at bottom, 270° arc

    const hasRange = minSpeed !== maxSpeed;

    // Segments: one per speed step, e.g. min=1 max=4 → 4 segments
    const segmentCount = hasRange ? maxSpeed - minSpeed + 1 : 1;
    const gapDeg = segmentCount > 1 ? 4 : 0;
    const segmentDeg = (270 - gapDeg * (segmentCount - 1)) / segmentCount;
    const segmentLen = circumference * (segmentDeg / 360);

    const dim = 'rgba(200, 168, 75, 0.18)';
    const amber = '#c8a84b';
    const amberLight = '#e8c96a';

    const autoColor   = autoControl   ? amber      : dim;
    const manualColor = manualControl ? amber      : dim;
    const lockedColor = locked        ? amberLight : dim;

    return (
        <div className="thread-status">
            <svg
                viewBox={`0 0 ${size} ${size}`}
                style={{ width: 'var(--pb-size, 8rem)', height: 'var(--pb-size, 8rem)', overflow: 'visible' }}
                overflow="visible"
            >
                {/* ── Segmented speedometer — one arc per speed step ── */}
                {Array.from({ length: segmentCount }, (_, i) => {
                    const startDeg = rotation + i * (segmentDeg + gapDeg);
                    const filled = (minSpeed + i) <= currentSpeed;
                    const speedNum = minSpeed + i;
                    return (
                        <circle
                            key={i}
                            cx={cx} cy={cy} r={radius}
                            fill="none"
                            stroke={filled ? amber : dim}
                            strokeWidth={strokeWidth}
                            strokeLinecap="butt"
                            strokeDasharray={`${segmentLen} ${circumference}`}
                            transform={`rotate(${startDeg} ${cx} ${cy})`}
                            style={{ cursor: 'pointer' }}
                            onClick={() => setThreadSpeed(speedNum)}
                            {...tooltipOn(`Speed ${speedNum}`)}
                        />
                    );
                })}

                {/* ── Dark inner backdrop ── */}
                <circle
                    cx={cx} cy={cy}
                    r={radius - strokeWidth / 2 - 2}
                    fill="rgba(0, 0, 0, 0.38)"
                />

                {/* ── Speed value text (center) ── */}
                <text
                    x={cx} y={cy}
                    textAnchor="middle"
                    dominantBaseline="middle"
                    className="thread-speed-value"
                    fill={amber}
                >
                    {currentSpeed}
                </text>

                {/* ── Lock icon — top (12 o'clock) ── */}
                <g {...tooltipOn('Locked Control')}>
                    {/* Invisible hit area */}
                    <rect x="51" y="18" width="18" height="17" rx="2" fill="transparent" />
                    {/* Shackle */}
                    <path
                        d="M56.5,25 V21 A3.5,3.5 0 0,1 63.5,21 V25"
                        fill="none"
                        stroke={lockedColor}
                        strokeWidth="2.2"
                        strokeLinecap="round"
                    />
                    {/* Body */}
                    <rect x="54" y="24.5" width="12" height="8" rx="1.5" fill={lockedColor} />
                </g>

                {/* ── Auto icon — play ▶, bottom-left (~7 o'clock) ── */}
                <g {...tooltipOn('Auto Control')}>
                    {/* Invisible hit area */}
                    <rect x="32" y="82" width="22" height="18" rx="2" fill="transparent" />
                    <polygon points="36,85 36,95 49,90" fill={autoColor} />
                </g>

                {/* ── Manual icon — reticle ⊙, bottom-right (~5 o'clock) ── */}
                <g {...tooltipOn('Manual Control')}>
                    {/* Invisible hit area */}
                    <circle cx="78" cy="90" r="11" fill="transparent" />
                    <circle cx="78" cy="90" r="6.5"
                        fill="none" stroke={manualColor} strokeWidth="2.2"
                    />
                    <circle cx="78" cy="90" r="2.2" fill={manualColor} />
                </g>
            </svg>

            {/* Speed range label below ring */}
            <div className="thread-label">
                <span className="thread-speed-range">
                    {hasRange ? `Min ${minSpeed} - Max ${maxSpeed}` : ''}
                </span>
            </div>

            <Tooltip {...tooltip} />
        </div>
    );
}
