import { ProgressBarContainer } from './components/ProgressBar/ProgressBarContainer';
import { Navigation } from './components';



function App() {
  console.log('Viewport size:', window.innerWidth, 'x', window.innerHeight)
  return (
    <div className="ui-container">
        <ProgressBarContainer />
        <Navigation />
    </div>
  )
}

export default App
