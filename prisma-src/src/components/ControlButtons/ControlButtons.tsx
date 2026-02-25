import { CSSProperties, FC, useLayoutEffect, useRef, useState } from "react";
import { useOStimStore } from "../../store";
import { Icon } from "../Icon/Icon";

import './ControlButtons.styles.css';

export const ControlButtons: FC = () => {
    const controlButtons = useOStimStore(state => state.controlButtons.buttons);
    const activeIndex = useOStimStore(state => state.controlButtons.activeIndex);
    const setButtonsActiveIndex = useOStimStore(state => state.setButtonsActiveIndex);
    const updateActiveMenu = useOStimStore(state => state.updateActiveMenu);
    const focusBlock = useOStimStore(state => state.focusBlock);

    const [hoverIndex, setHoverIndex] = useState<number | null>(null);
    const buttonRefs = useRef<(HTMLDivElement | null)[]>([]);
    const containerRef = useRef<HTMLDivElement>(null);
    const [indicatorStyle, setIndicatorStyle] = useState<CSSProperties>({ opacity: 0 });

    const targetIndex = hoverIndex ?? activeIndex;

    useLayoutEffect(() => {
        setTimeout(() => {
            const el = buttonRefs.current[targetIndex];
            if (!el) { setIndicatorStyle({ opacity: 0 }); return; }
            setIndicatorStyle({
                top: el.offsetTop,
                left: el.offsetLeft,
                width: el.offsetWidth,
                height: el.offsetHeight,
                opacity: 1,
            });
        }, 0)
        
    }, [targetIndex, controlButtons]);

    return (
        <div ref={containerRef} className={`control-buttons ${focusBlock === 'buttons' ? 'focused' : ''}`}>
            <div className="control-button-indicator" style={indicatorStyle} />
            {controlButtons.map((button, index) => (
                <div
                    key={button.id}
                    ref={el => { buttonRefs.current[index] = el; }}
                    className={`control-button ${index === activeIndex ? 'active' : ''}`}
                    onClick={() => {
                        setButtonsActiveIndex(index);
                        updateActiveMenu(button.id);
                    }}
                    onMouseEnter={() => setHoverIndex(index)}
                    onMouseLeave={() => setHoverIndex(null)}
                >
                    <div style={{ width: 'var(--control-buttons-size)', height: 'var(--control-buttons-size)' }}>
                        <Icon base64Data={button.iconData} path={button.iconPath} size="var(--control-buttons-size)" />
                    </div>
                    
                    <div className="control-button-label">{button.description}</div>
                </div>
            ))}
        </div>
    );
}