import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import './index.css'
import { useOStimStore } from './store'

// Register BEFORE React renders - these are synchronous
(window as any).updateNavigation = (navigationOptions: string | any[]) => {
  console.log("updateNavigation called with:", navigationOptions);
  const options = typeof navigationOptions === 'string' 
    ? JSON.parse(navigationOptions) 
    : navigationOptions;
  useOStimStore.getState().updateNavigationOptions(options);
};

(window as any).updateExcitements = (data: string | any[]) => {
  const actorsState = typeof data === 'string' ? JSON.parse(data) : data;
  useOStimStore.getState().updateActorsState(actorsState);
};

// (window as any).prismaReady = (source: string) => {
//   console.log('prismaReady called from:', source);
//   console.log('sendAction exists:', typeof (window as any).sendAction);
//   if (typeof (window as any).sendAction === 'function') {
//     (window as any).sendAction(JSON.stringify({ action: "navigationInit" }));
//   } else {
//     console.error('sendAction not available yet!');
//   }
// };

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>,
)
