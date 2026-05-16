console.log('%c[Boot] ux/src/index.jsx executing...', 'background: #222; color: #ff00ff; font-weight: bold;');
console.log('%c[Boot] Entry point reached.', 'background: #222; color: #bada55');
console.log('[Boot] Current Hash:', window.location.hash || 'None');

import { render } from 'solid-js/web';
import { ErrorBoundary, onMount } from 'solid-js';
import { Canvas } from './components/canvas/Canvas';
import { ErrorOverlay } from './components/system/ErrorOverlay';
import { blackboard } from './lib/blackboard';
import { RemoteStorageHandler } from './lib/vfs/RemoteStorageHandler';
import { logActions } from './lib/state/LogState';
import { initAppState } from './lib/state/SystemState';
import './index.css';

console.log('[Boot] Modules imported.');

// Initialize Logging, App State, and Cloud Sync
try {
    console.log('[Boot] Initializing App State...');
    initAppState();
    console.log('[Boot] Initializing RemoteStorage...');
    RemoteStorageHandler.init();
    console.log('[Boot] System Ready.');
} catch (e) {
    console.error('[Boot] Initialization failed:', e);
}

function App() {
    console.log('[Boot] Rendering App component.');
  // 1. Global Window Listeners
  window.addEventListener('error', (event) => {
    console.error('[Global Error]', event.error || event.message);
    if (event.error) {
        blackboard.setError(event.error);
    } else {
        blackboard.setError(new Error(event.message || 'Unknown Global Error'));
    }
  });

  window.addEventListener('unhandledrejection', (event) => {
    const reason = event.reason;
    console.error('[Global Promise Rejection]', reason);
    
    // Ignore Vite HMR WebSocket errors so they don't crash the app
    if (reason && (reason.message?.includes('WebSocket') || (reason instanceof Event && reason.type === 'error'))) {
        console.warn('[Ignored] Vite HMR WebSocket or generic Event Error');
        return;
    }

    if (reason instanceof Error) {
        blackboard.setError(reason);
    } else {
        // ULTRA SAFE: No JSON.stringify here
        blackboard.setError(new Error(String(reason || 'Unhandled Promise Rejection')));
    }
  });

  onMount(() => {
    console.log('%c[App] Component Mounted.', 'color: #00ff00; font-weight: bold;');
  });

  // Expose for manual reporting from modules
  window.reportError = (err) => blackboard.setError(err instanceof Error ? err : new Error(String(err)));

  return (
    <div class="w-screen h-screen overflow-hidden">
      <ErrorBoundary fallback={(err) => {
        blackboard.setError(err);
        return <div class="w-full h-full bg-slate-950" />; // Empty placeholder
      }}>
        <Canvas />
      </ErrorBoundary>
      <ErrorOverlay />
    </div>
  );
}

const root = document.getElementById('root');
if (root) {
  render(() => <App />, root);
}
