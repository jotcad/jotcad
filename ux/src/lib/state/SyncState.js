// console.log('[Trace] Executing SyncState.js');
import { createSignal } from 'solid-js';
import { createStore } from 'solid-js/store';

export const SHADOW_OPS_KEY = 'jot_shadow_ops';
export const SHADOW_LAYOUT_KEY = 'jot_shadow_layout';

// BASE (Shadow) State - This reflects what is currently on the cloud
const getInitialShadowOps = () => {
    try {
        const saved = localStorage.getItem(SHADOW_OPS_KEY);
        return saved ? JSON.parse(saved) : {};
    } catch (e) { return {}; }
};

const getInitialShadowLayout = () => {
    try {
        const saved = localStorage.getItem(SHADOW_LAYOUT_KEY);
        const parsed = saved ? JSON.parse(saved) : { windows: [], desktop: [] };
        return {
            windows: Array.isArray(parsed.windows) ? parsed.windows : [],
            desktop: Array.isArray(parsed.desktop) ? parsed.desktop : []
        };
    } catch (e) { return { windows: [], desktop: [] }; }
};

export const [shadowOps, setShadowOps] = createStore(getInitialShadowOps());
export const [shadowLayout, setShadowLayout] = createStore(getInitialShadowLayout());

// Sync Status Signals
export const [syncStatus, setSyncStatus] = createSignal('idle'); // 'idle' | 'syncing' | 'conflict' | 'error'
export const [isDirty, setIsDirty] = createSignal(false);
export const [lastSyncTime, setLastSyncTime] = createSignal(Date.now());
export const [cloudAccount, setCloudAccount] = createSignal(null); 

export const syncActions = {
    markDirty() {
        setIsDirty(true);
    },

    updateShadowOp(path, op) {
        setShadowOps(path, op);
        localStorage.setItem(SHADOW_OPS_KEY, JSON.stringify(shadowOps));
    },

    removeShadowOp(path) {
        setShadowOps(path, undefined);
        localStorage.setItem(SHADOW_OPS_KEY, JSON.stringify(shadowOps));
    },

    updateShadowLayout(layout) {
        const clean = {
            windows: Array.isArray(layout.windows) ? layout.windows : [],
            desktop: Array.isArray(layout.desktop) ? layout.desktop : []
        };
        setShadowLayout(clean);
        localStorage.setItem(SHADOW_LAYOUT_KEY, JSON.stringify(clean));
    },

    clearShadow() {
        setShadowOps({});
        setShadowLayout({ windows: [], desktop: [] });
        localStorage.removeItem(SHADOW_OPS_KEY);
        localStorage.removeItem(SHADOW_LAYOUT_KEY);
        setIsDirty(false);
    }
};
