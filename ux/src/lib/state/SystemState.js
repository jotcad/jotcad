// console.log('[Trace] Executing SystemState.js');
import { createSignal } from 'solid-js';
import { initDesktopState } from './DesktopState';
import { logActions } from './LogState';

export const [pointerCount, setPointerCount] = createSignal(0);
export const [isGesturing, setIsGesturing] = createSignal(false);
export const [gestureOwner, setGestureOwner] = createSignal(null); 
export const [error, setError] = createSignal(null);

export const [viewportSettings, setViewportSettings] = createSignal({
  hitchhikerGrid: false
});

export const initAppState = () => {
  logActions.initInterception();
  initDesktopState();
};
