import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import './index.css'
import { useOStimStore } from './store'
import { setupGameIntegration } from './services/gameIntegration'

// Initialize game integration with store instance
const store = useOStimStore.getState();
setupGameIntegration(store);

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
