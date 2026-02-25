import { ProgressBarContainer } from './components/ProgressBar/ProgressBarContainer';
import { SearchMenu, AlignmentMenu, NavigationList, UtilityOptionsList} from './components';
import { CSSProperties, useEffect } from 'react';
import { useOStimStore } from './store';
import { useControls } from './hooks/useControls';
import { ControlButtons } from './components/ControlButtons/ControlButtons';
import { isInGame } from './utils/isInGame';



function App() {
  const updateActiveMenu = useOStimStore(state => state.updateActiveMenu);
  const activeMenu = useOStimStore(state => state.activeMenu);
  const focusBlock = useOStimStore(state => state.focusBlock);
  useEffect(() => {
    updateActiveMenu('navigation');
  }, []);

  useControls();

  const inGame = isInGame();
  const style: CSSProperties = !inGame ? { backgroundImage: 'url("./ScreenShot183.png")' } : {};

  return (
    <div className={`ui-container ${focusBlock === 'buttons' ? 'focus-buttons' : 'focus-menu'}`} style={style}>
        <div className="buttons-section">
          <ControlButtons />       
        </div>
        <ProgressBarContainer />
        <div className="menu-section">
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
    </div>
  )
}

export default App
