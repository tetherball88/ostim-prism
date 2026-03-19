import { FC, useEffect, useRef, useState } from 'react';
import { useTooltip } from '../../hooks/useTooltip';
import { ActorStats } from '../../types';
import { Tooltip } from '../Tooltip/Tooltip';
import './ProgressBar.styles.css';
import { useOStimStore } from '../../store';
import { isInGame } from '../../utils/isInGame';

type ProgressBarProps = {
  index: number;
}

const genderColors: Record<ActorStats['gender'], string> = {
  male: '#6495ED',
  female: '#F48FB1',
  neither: '#B39DDB'
}

export const ProgressBar: FC<ProgressBarProps> = ({ index }) => {
  const updateActorsState = useOStimStore(state => state.updateActorsState);
  const actors = useOStimStore(state => state.actorsState);
  const name = useOStimStore(state => state.actorsState[index].name);
  const gender = useOStimStore(state => state.actorsState[index].gender);
  const rawExcitementProgress = useOStimStore(state => state.actorsState[index].excitementProgress);
  const staminaProgress = useOStimStore(state => state.actorsState[index].staminaProgress);
  const timesClimaxed = useOStimStore(state => state.actorsState[index].timesClimaxed);
  let additionalProgress = useOStimStore(state => state.actorsState[index].additionalProgress);
  const orgasmPinnedUntil = useOStimStore(state => state.orgasmPinnedUntil[index] ?? 0);

  // While orgasm pin is active, hold excitement at 100
  const [isPinned, setIsPinned] = useState(false);
  const pinTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  // RAF-based animated progress (avoids strokeDasharray — Ultralight incompatible)
  const animRef = useRef({ excitement: 0, stamina: 0, additional: 0 });
  const targetRef = useRef({ excitement: 0, stamina: 0, additional: 0 });
  const rafRef = useRef<number | null>(null);
  const [animProgress, setAnimProgress] = useState({ excitement: 0, stamina: 0, additional: 0 });

  useEffect(() => {
    if (!orgasmPinnedUntil) return;
    const remaining = orgasmPinnedUntil - Date.now();
    if (remaining <= 0) return;
    setIsPinned(true);
    if (pinTimerRef.current) clearTimeout(pinTimerRef.current);
    pinTimerRef.current = setTimeout(() => setIsPinned(false), remaining);
    return () => {
      if (pinTimerRef.current) clearTimeout(pinTimerRef.current);
    };
  }, [orgasmPinnedUntil]);

  const excitementProgress = isPinned ? 100 : Math.max(rawExcitementProgress, 0);

  if(typeof additionalProgress !== 'number') {
    additionalProgress = -1
  }

  useEffect(() => {
    targetRef.current = {
      excitement: excitementProgress,
      stamina: staminaProgress,
      additional: Math.max(additionalProgress as number, 0),
    };

    const SPEED = 0.05;
    const SNAP = 0.1;

    const lerp = (cur: number, tgt: number): { val: number; moving: boolean } => {
      const diff = tgt - cur;
      if (Math.abs(diff) <= SNAP) return { val: tgt, moving: false };
      return { val: cur + diff * SPEED, moving: true };
    };

    const tick = () => {
      const e = lerp(animRef.current.excitement, targetRef.current.excitement);
      const s = lerp(animRef.current.stamina, targetRef.current.stamina);
      const a = lerp(animRef.current.additional, targetRef.current.additional);
      animRef.current = { excitement: e.val, stamina: s.val, additional: a.val };
      setAnimProgress({ ...animRef.current });
      if (e.moving || s.moving || a.moving) {
        rafRef.current = requestAnimationFrame(tick);
      } else {
        rafRef.current = null;
      }
    };

    if (rafRef.current !== null) cancelAnimationFrame(rafRef.current);
    rafRef.current = requestAnimationFrame(tick);

    return () => {
      if (rafRef.current !== null) {
        cancelAnimationFrame(rafRef.current);
        rafRef.current = null;
      }
    };
  }, [excitementProgress, staminaProgress, additionalProgress]);

  const size = 120;
  const cx = size / 2;
  const strokeWidth = 14;
  const radius = (size - strokeWidth) / 2;

  // Rotation: 135° puts the start at bottom-left, arc goes clockwise to bottom-right
  const rotation = 135;

  // const highThreshold = 75
  const highThreshold = 75;
  const isHighProgress = excitementProgress >= highThreshold;

  // Pulse speed: 2s at 75%, scales down to 0.5s at 100%
  const pulseDuration = Math.max(isHighProgress
    ? 2 - ((excitementProgress - highThreshold) / 25) * 1.5
    : 0, 0.5);

  // Outer ring - same horseshoe shape, split at top
  const outerStrokeWidth = 6;
  const outerRadius = radius + strokeWidth / 2 + outerStrokeWidth / 2;

  const color = genderColors[gender];

  const { tooltip, showTooltip, hideTooltip } = useTooltip();

  /** Build an SVG arc path for radius r from startDeg to endDeg. */
  const describeArc = (r: number, startDeg: number, endDeg: number, clockwise = true): string => {
    const toRad = (d: number) => (d * Math.PI) / 180;
    const x1 = cx + r * Math.cos(toRad(startDeg));
    const y1 = cx + r * Math.sin(toRad(startDeg));
    const x2 = cx + r * Math.cos(toRad(endDeg));
    const y2 = cx + r * Math.sin(toRad(endDeg));
    const sweep = clockwise ? 1 : 0;
    const largeArc = Math.abs(endDeg - startDeg) > 180 ? 1 : 0;
    return `M ${x1} ${y1} A ${r} ${r} 0 ${largeArc} ${sweep} ${x2} ${y2}`;
  };

  /**
   * Geometric mouse handler — no SVG hit-testing involved.
   * Checks radial distance to pick inner/outer ring, then angle for the segment.
   */
  const handleMouseMove = (e: React.MouseEvent<SVGSVGElement>) => {
    const rect = e.currentTarget.getBoundingClientRect();
    const vx = (e.clientX - rect.left) * (size / rect.width);
    const vy = (e.clientY - rect.top)  * (size / rect.height);
    const dx = vx - cx;
    const dy = vy - cx;
    const dist = Math.sqrt(dx * dx + dy * dy);

    const innerMin = radius - strokeWidth / 2;
    const innerMax = radius + strokeWidth / 2;
    const outerMin = outerRadius - outerStrokeWidth / 2;
    const outerMax = outerRadius + outerStrokeWidth / 2;

    // Center zone → climax icon
    if (dist < innerMin) {
      showTooltip(`Orgasmed ${timesClimaxed} times`, e.clientX, e.clientY);
      return;
    }

    let angle = Math.atan2(dy, dx) * (180 / Math.PI);
    if (angle < 0) angle += 360;
    // Degrees relative to ring start (rotation), clockwise
    let rel = angle - rotation;
    if (rel < 0) rel += 360;

    if (dist >= innerMin && dist <= innerMax) {
      // Inner ring: excitement spans 0°–270° relative
      if (rel <= 270) {
        showTooltip(`Excitement ${Math.round(excitementProgress)}%`, e.clientX, e.clientY);
      } else {
        hideTooltip();
      }
      return;
    }

    if (dist >= outerMin && dist <= outerMax) {
      if (rel <= 135) {
        showTooltip(`Stamina ${Math.round(staminaProgress)}%`, e.clientX, e.clientY);
      } else if (rel <= 270 && additionalProgress !== -1) {
        showTooltip(`Cum ${Math.round(additionalProgress)}%`, e.clientX, e.clientY);
      } else {
        hideTooltip();
      }
      return;
    }

    hideTooltip();
  };

  const innerFillEnd = rotation + 270 * (animProgress.excitement / 100);
  const outerLeftFillEnd = rotation + 135 * (animProgress.stamina / 100);
  const outerRightFillEnd = rotation + 270 - 135 * (animProgress.additional / 100);

  return (
    <div
      onClick={() => {
        if(!isInGame()) {
          updateActorsState(actors.map((actor, i) => {
            if(i === index) {
              const newValue = actor.excitementProgress + 10 > 100 ? 0 : actor.excitementProgress + 10
              return {
                ...actor,
                excitementProgress: newValue,
                staminaProgress: newValue,
                additionalProgress: newValue,
              }
            } else {
              return actor
            }
          }))
        }          
      }}
      className={`circular-progress${isHighProgress ? ' high-progress' : ''}`}
      style={{ '--pulse-duration': `${pulseDuration}s`, '--glow-color': color } as React.CSSProperties}
    >
      <svg
        viewBox={`0 0 ${size} ${size}`}
        style={{ width: 'var(--pb-size, 8rem)', height: 'var(--pb-size, 8rem)', overflow: 'visible' }}
        onMouseMove={handleMouseMove}
        onMouseLeave={hideTooltip}
      >
        {/* Outer ring - green segment (left half: bottom-left to top) */}
        <path
          className="outer-ring-left-bg"
          d={describeArc(outerRadius, rotation, rotation + 135)}
          strokeWidth={outerStrokeWidth}
          stroke="#95ed6480"
          fill="none"
        />
        {animProgress.stamina > 0 && (
          <path
            className="outer-ring-left"
            d={describeArc(outerRadius, rotation, outerLeftFillEnd)}
            strokeWidth={outerStrokeWidth}
            stroke="#95ed64"
            fill="none"
            style={{ pointerEvents: 'none' }}
          />
        )}
        {
          additionalProgress === -1 ? null : (
            <>
              {/* Outer ring - ivory segment (right half: top to bottom-right) */}
              <path
                className="outer-ring-right-bg"
                d={describeArc(outerRadius, rotation + 135, rotation + 270)}
                strokeWidth={outerStrokeWidth}
                stroke="#C5C5C5"
                fill="none"
              />
              {animProgress.additional > 0 && (
                <path
                  className="outer-ring-right"
                  d={describeArc(outerRadius, rotation + 270, outerRightFillEnd, false)}
                  strokeWidth={outerStrokeWidth}
                  stroke="#FFFFF0"
                  fill="none"
                  style={{ pointerEvents: 'none' }}
                />
              )}
            </>
          )
        }
        <path
          className="progress-bg"
          d={describeArc(radius, rotation, rotation + 270)}
          stroke={`${color}80`}
          strokeWidth={strokeWidth}
          fill="none"
        />
        {animProgress.excitement > 0 && (
          <path
            className="progress-fill"
            d={describeArc(radius, rotation, innerFillEnd)}
            strokeWidth={strokeWidth}
            stroke={color}
            fill="none"
            style={{ color, pointerEvents: 'none' }}
          />
        )}
        {/* Climax icon — tooltip handled geometrically via SVG onMouseMove */}
        <g>
          {/* Invisible hit area spanning the whole icon+text row */}
          <rect
            x={size / 4}
            y={size / 4}
            width={size / 2}
            height={size / 2}
            fill="transparent"
          />
          <svg
            x={size / 2 - size / 4 + 2}
            y={size / 2 - size / 8 - 4}
            viewBox="0 0 2048 2048"
            width={size/4}
            height={size/4}
            preserveAspectRatio="none"
            fill={color}
            style={{ pointerEvents: 'none' }}
          >
            <path d="M 275.969 505.424 C 306.487 505.906 325.269 520.872 337.506 548.313 C 344.807 564.685 353.604 582.304 362.034 598.148 C 378.607 628.871 396.586 658.814 415.915 687.882 C 486.926 793.2 568.779 890.786 660.133 979.04 C 694.409 1012.4 729.589 1044.82 765.634 1076.26 C 827.541 1130.18 869.496 1156.28 911.337 1229.56 C 961.217 1317.45 974.281 1421.51 947.673 1519.01 C 918.658 1624.23 858.178 1691.61 764.715 1744.56 C 714.741 1769.88 665.907 1786.24 609.78 1791.86 C 515.706 1801.33 420.998 1781.58 338.556 1735.28 C 201.519 1659.7 143.692 1525.52 129.615 1374.84 C 126.26 1338.93 134.998 1289.97 138.939 1252.94 L 166.277 1008.79 L 200.416 705.314 L 211.103 610.48 C 216.385 563.243 214.17 510.623 275.969 505.424 z"/>
            <path d="M 1472.77 239.457 C 1498.12 238.883 1523.48 239.931 1548.69 242.597 C 1659.46 254.751 1771.28 301.31 1841.91 390.184 C 1904.08 468.421 1928.61 568.21 1917.57 666.782 C 1904.8 771.775 1851.19 867.511 1768.33 933.249 C 1629.77 1043.27 1448.5 1038.99 1315.89 922.247 C 1299.22 907.566 1284.84 888.747 1271.15 871.334 C 1254.69 848.951 1234.65 823.973 1213.55 805.7 C 1081.56 691.409 933.871 583.872 765.956 530.171 C 743.707 523.056 718.102 520.88 700.45 504.86 C 688.292 493.754 681.055 478.266 680.335 461.814 C 679.572 444.644 685.194 427.696 696.791 414.852 C 713.15 396.732 737.722 394.078 760.491 389.066 L 818.201 376.237 L 1007.55 334.059 L 1262.5 277.171 L 1349.71 257.68 C 1395.46 247.473 1425.82 241.701 1472.77 239.457 z"/>
            <path d="M 1191.7 1053.39 C 1212.36 1052.95 1227.01 1061.29 1244.65 1070.74 C 1264.13 1081.16 1283.68 1091.54 1303.18 1101.93 L 1450.47 1180.34 L 1557.31 1237.26 C 1581.91 1250.36 1610.4 1264.65 1633.09 1280.27 C 1662.75 1300.56 1688.65 1325.88 1709.6 1355.07 C 1755.32 1418.81 1773.71 1498.16 1760.69 1575.51 C 1748.12 1650.18 1707.02 1712.45 1645.56 1755.9 C 1607.78 1782.08 1548.9 1806.72 1503.21 1811.23 C 1437.3 1817.73 1374.7 1795.85 1323.94 1753.79 C 1283.64 1720.07 1252.9 1676.37 1234.79 1627.06 C 1218.86 1584.01 1218.01 1556.93 1214.03 1512.94 C 1209.53 1463.23 1206.57 1409.73 1197.23 1360.8 C 1194.13 1344.6 1183.66 1314.24 1178.43 1297.68 L 1144.8 1191.45 C 1139.23 1173.85 1128.47 1143.5 1126.96 1126.56 C 1125.42 1108.55 1131.17 1090.68 1142.92 1076.96 C 1155.96 1061.6 1172.05 1055.14 1191.7 1053.39 z"/>
          </svg>
          <text
            className="progress-percentage"
            x={size / 2 + 6}
            y={size / 2}
            textAnchor="start"
            dominantBaseline="middle"
            fill={color}
            fontSize={size/3}
            style={{ pointerEvents: 'none' }}
          >
            {timesClimaxed}
          </text>
        </g>
      </svg>
      <div className="progress-label">
        <span className="progress-name">{name}</span>
      </div>
      <Tooltip {...tooltip} />
    </div>
  );
}
