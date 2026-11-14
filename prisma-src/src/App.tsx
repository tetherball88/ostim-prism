import { ProgressBarContainer } from './components/ProgressBar/ProgressBarContainer';
import { SearchMenu, AlignmentMenu, NavigationList, UtilityOptionsList} from './components';
import { CSSProperties, useEffect, useRef } from 'react';
import { useOStimStore } from './store';
import { useControls } from './hooks/useControls';
import { ControlButtons } from './components/ControlButtons/ControlButtons';
import { BlockLayoutControls } from './components/BlockLayoutControls/BlockLayoutControls';
import { isInGame } from './utils/isInGame';



function App() {
  const updateActiveMenu = useOStimStore(state => state.updateActiveMenu);
  const activeMenu = useOStimStore(state => state.activeMenu);
  const focusBlock = useOStimStore(state => state.focusBlock);
  const settings = useOStimStore(state => state.settings);
  const updateSettings = useOStimStore(state => state.updateSettings);
  const saveLayoutSettings = useOStimStore(state => state.saveLayoutSettings);
  const actorPortraits = useOStimStore(state => state.actorPortraits);
  const updateActorPortrait = useOStimStore(state => state.updateActorPortrait);
  const clearActorPortraits = useOStimStore(state => state.clearActorPortraits);
  const hasMockPortraits = Object.keys(actorPortraits).length > 0;
  const mainBlockRef = useRef<HTMLDivElement | null>(null);
  useEffect(() => {
    updateActiveMenu('navigation');
  }, []);

  useControls();

  const inGame = isInGame();
  const style: CSSProperties = !inGame ? { backgroundImage: 'url("./assets/background.jpg")' } : {};

  return (
    <div className={`ui-container ${focusBlock === 'buttons' ? 'focus-buttons' : 'focus-menu'}`} style={style}>
        {!inGame && (
          <div style={{
            position: 'fixed',
            top: '12px',
            right: '12px',
            zIndex: 10000,
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
            padding: '8px 12px',
            background: 'rgba(14, 12, 8, 0.88)',
            border: '1px solid var(--amber)',
            fontSize: '13px',
          }}>
            <button
              onClick={() => updateSettings({ enableGridMenu: !settings.enableGridMenu })}
              style={{
                padding: '4px 10px',
                background: 'var(--amber-glow)',
                border: '1px solid var(--amber)',
                color: '#fff',
                cursor: 'pointer',
                textTransform: 'uppercase',
              }}
            >
              Layout: {settings.enableGridMenu ? 'Grid' : 'List'}
            </button>
            {settings.enableGridMenu && (
              <>
                <label style={{ display: 'flex', alignItems: 'center', gap: '6px', color: 'var(--amber)' }}>
                  Cols:
                  <input
                    type="number"
                    min={1}
                    max={12}
                    value={settings.gridColumns}
                    onChange={(e) => updateSettings({ gridColumns: Number(e.target.value) })}
                    style={{
                      width: '48px',
                      padding: '3px 6px',
                      background: '#111',
                      border: '1px solid var(--amber-dim)',
                      color: '#fff',
                    }}
                  />
                </label>
                <label style={{ display: 'flex', alignItems: 'center', gap: '6px', color: 'var(--amber)' }}>
                  Rows:
                  <input
                    type="number"
                    min={1}
                    max={12}
                    value={settings.gridRows}
                    onChange={(e) => updateSettings({ gridRows: Number(e.target.value) })}
                    style={{
                      width: '48px',
                      padding: '3px 6px',
                      background: '#111',
                      border: '1px solid var(--amber-dim)',
                      color: '#fff',
                    }}
                  />
                </label>
              </>
            )}
            <button
              onClick={() => {
                if (hasMockPortraits) {
                  clearActorPortraits();
                } else {
                  [1, 2, 3].forEach((id) => {
                    updateActorPortrait(id, './assets/icons/placeholder.png');
                  });
                }
              }}
              style={{
                padding: '4px 10px',
                background: 'var(--amber-glow)',
                border: '1px solid var(--amber)',
                color: '#fff',
                cursor: 'pointer',
                textTransform: 'uppercase',
              }}
            >
              Portraits: {hasMockPortraits ? 'On' : 'Off'}
            </button>
          </div>
        )}
        <div
          ref={mainBlockRef}
          className="hud-block hud-main-block"
          style={{
            left: settings.mainBlockX >= 0 ? `${settings.mainBlockX}px` : '1vw',
            bottom: settings.mainBlockY >= 0 ? `${settings.mainBlockY}px` : 'var(--hud-bottom)',
            transform: `scale(${settings.mainBlockScale / 100})`,
            transformOrigin: 'bottom left',
          }}
        >
          <div className="buttons-section">
            <ControlButtons />       
          </div>
          <div className={`menu-section ${(activeMenu === 'navigation' || activeMenu === 'utilityOptions') && settings.enableGridMenu ? 'grid-mode' : ''}`}>
            {
              activeMenu === 'navigation' && <NavigationList />
            }
            {
              activeMenu === 'utilityOptions' && <UtilityOptionsList />
            }
            {
              activeMenu === 'searchMenu' && <SearchMenu />
            }
            {
              activeMenu === 'alignMenu' && <AlignmentMenu />
            }
          </div>
          <BlockLayoutControls
            blockRef={mainBlockRef}
            scale={settings.mainBlockScale}
            placement="bottom-left"
            enableGridMenu={settings.enableGridMenu}
            gridColumns={settings.enableGridMenu ? settings.gridColumns : undefined}
            gridRows={settings.enableGridMenu ? settings.gridRows : undefined}
            onChange={({ x, y, scale, enableGridMenu, gridColumns, gridRows }, commit) =>
              saveLayoutSettings(
                {
                  ...(x !== undefined ? { mainBlockX: x } : {}),
                  ...(y !== undefined ? { mainBlockY: y } : {}),
                  ...(scale !== undefined ? { mainBlockScale: scale } : {}),
                  ...(enableGridMenu !== undefined ? { enableGridMenu } : {}),
                  ...(gridColumns !== undefined ? { gridColumns } : {}),
                  ...(gridRows !== undefined ? { gridRows } : {}),
                },
                commit
              )
            }
          />
        </div>
        <ProgressBarContainer />
    </div>
  )
}

export default App
