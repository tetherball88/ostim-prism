import { ActorStats } from '../../types';
import './ProgressBar.styles.css';

type ProgressBarProps = ActorStats

const genderColors: Record<ActorStats['gender'], string> = {
  male: '#6495ED',
  female: '#F48FB1',
  neither: '#B39DDB'
}

export function ProgressBar({ name, excitementProgress, gender, staminaProgress, additionalProgress }: ProgressBarProps) {
  if(typeof additionalProgress !== 'number') {
    additionalProgress = -1
  }

  const size = 120;
  const strokeWidth = 12;
  const radius = (size - strokeWidth) / 2;
  const circumference = radius * 2 * Math.PI;

  // Horseshoe: 270 degrees (75% of circle), gap at bottom
  const arcFraction = 0.75;
  const arcLength = circumference * arcFraction;

  // Progress fill: only draw the filled portion
  const filledLength = (excitementProgress / 100) * arcLength;

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
  const outerStrokeWidth = 8;
  const outerRadius = radius + strokeWidth / 2 + outerStrokeWidth / 2;
  const outerCircumference = outerRadius * 2 * Math.PI;
  const outerArcLength = outerCircumference * arcFraction; // 270°
  const halfOuterArc = outerArcLength / 2; // 135° each segment

  const leftFilledLength = (staminaProgress / 100) * halfOuterArc;
  const rightFilledLength = (additionalProgress / 100) * halfOuterArc;

  const color = genderColors[gender];

  return (
    <div
      className={`circular-progress${isHighProgress ? ' high-progress' : ''}`}
      style={{ '--pulse-duration': `${pulseDuration}s`, '--glow-color': color } as React.CSSProperties}
    >
      <svg width={size} height={size} style={{ overflow: 'visible' }}>
        {/* Outer ring - green segment (left half: bottom-left to top) */}
        <circle
          className="outer-ring-left-bg"
          cx={size / 2}
          cy={size / 2}
          r={outerRadius}
          strokeWidth={outerStrokeWidth}
          stroke="#95ed6480"
          fill="none"
          strokeDasharray={`${halfOuterArc} ${outerCircumference}`}
          transform={`rotate(${rotation} ${size / 2} ${size / 2})`}
        />
        <circle
          className="outer-ring-left"
          cx={size / 2}
          cy={size / 2}
          r={outerRadius}
          strokeWidth={outerStrokeWidth}
          stroke="#95ed64"
          fill="none"
          strokeDasharray={`${leftFilledLength} ${outerCircumference}`}
          transform={`rotate(${rotation} ${size / 2} ${size / 2})`}
        />
        {
          additionalProgress === -1 ? null : (
            <>  
            {/* Outer ring - ivory segment (right half: top to bottom-right) */}
            <circle
              className="outer-ring-right-bg"
              cx={size / 2}
              cy={size / 2}
              r={outerRadius}
              strokeWidth={outerStrokeWidth}
              stroke="#C5C5C5"
              fill="none"
              strokeDasharray={`${halfOuterArc} ${outerCircumference}`}
              transform={`rotate(${rotation + 135} ${size / 2} ${size / 2})`}
            />
            <circle
              className="outer-ring-right"
              cx={size / 2}
              cy={size / 2}
              r={outerRadius}
              strokeWidth={outerStrokeWidth}
              stroke="#FFFFF0"
              fill="none"
              strokeDasharray={`0 ${halfOuterArc - rightFilledLength} ${rightFilledLength} ${outerCircumference}`}
              transform={`rotate(${rotation + 135} ${size / 2} ${size / 2})`}
            />
            </>
          )
        }
        <circle
          className="progress-bg"
          cx={size / 2}
          cy={size / 2}
          r={radius}
          stroke={`${color}80`}
          strokeWidth={strokeWidth}
          strokeDasharray={`${arcLength} ${circumference}`}
          transform={`rotate(${rotation} ${size / 2} ${size / 2})`}
        />
        <circle
          className="progress-fill"
          cx={size / 2}
          cy={size / 2}
          r={radius}
          strokeWidth={strokeWidth}
          stroke={color}
          style={{ color }}
          strokeDasharray={`${filledLength} ${circumference}`}
          transform={`rotate(${rotation} ${size / 2} ${size / 2})`}
        />
      </svg>
      <div className="progress-label">
        <span className="progress-name">{name}</span>
      </div>
    </div>
  );
}
