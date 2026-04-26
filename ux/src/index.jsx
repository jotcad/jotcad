import { render } from 'solid-js/web';
import { ErrorBoundary } from 'solid-js';
import { Canvas } from './components/Canvas';
import { ErrorOverlay } from './components/ErrorOverlay';
import { blackboard } from './lib/blackboard';
import './index.css';

function App() {
  // 1. Global Window Listeners
  window.addEventListener('error', (event) => {
    if (event.error) {
        blackboard.setError(event.error);
    } else {
        blackboard.setError(new Error(event.message || 'Unknown Global Error'));
    }
  });

  window.addEventListener('unhandledrejection', (event) => {
    const reason = event.reason;
    if (reason instanceof Error) {
        blackboard.setError(reason);
    } else {
        blackboard.setError(new Error(typeof reason === 'string' ? reason : JSON.stringify(reason) || 'Unhandled Promise Rejection'));
    }
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
