import { createSignal } from 'solid-js';
import { createStore } from 'solid-js/store';

const storagePrefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
export const SHADOW_OPS_KEY = `${storagePrefix}_shadow_ops`;
export const SHADOW_LAYOUT_KEY = `${storagePrefix}_shadow_layout`;

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
        return saved ? JSON.parse(saved) : {};
    } catch (e) { return {}; }
};

export const [shadowOps, setShadowOps] = createStore(getInitialShadowOps());
export const [shadowLayout, setShadowLayout] = createStore(getInitialShadowLayout());

// Sync Status Signals
export const [syncStatus, setSyncStatus] = createSignal('idle'); // 'idle' | 'syncing' | 'conflict' | 'error'
export const [lastSyncTime, setLastSyncTime] = createSignal(Date.now());
export const [cloudAccount, setCloudAccount] = createSignal(null); // { email, name, picture }

export const syncActions = {
    updateShadowOp(path, op) {
        setShadowOps(path, op);
        localStorage.setItem(SHADOW_OPS_KEY, JSON.stringify(shadowOps));
    },

    updateShadowLayout(layout) {
        setShadowLayout(layout);
        localStorage.setItem(SHADOW_LAYOUT_KEY, JSON.stringify(shadowLayout));
    },

    clearShadow() {
        setShadowOps({});
        setShadowLayout({});
        localStorage.removeItem(SHADOW_OPS_KEY);
        localStorage.removeItem(SHADOW_LAYOUT_KEY);
    }
};
