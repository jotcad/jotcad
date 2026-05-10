import { Merger } from './CloudSync';
import { shadowOps, shadowLayout, syncActions } from '../state/SyncState';
import { DYNAMIC_OPS_KEY, NODE_STATE_KEY, DESKTOP_STATE_KEY } from '../state/Config';

/**
 * Worksheet: The unified persistence and sync layer.
 * Manages LocalStorage + RemoteStorage with tier-specific merging.
 * 
 * NOTE: This module must not import blackboard or AppState directly 
 * to avoid circular dependencies.
 */
export const Worksheet = {
    TIERS: {
        OPERATORS: 'operators', // user/*.jot scripts
        DESKTOP: 'desktop',     // icon/folder layout
        WINDOWS: 'windows',     // screen-space window state
        ASSETS: 'assets',       // base64 icons/images
        APP_DATA: 'app_data',   // app-specific settings
        CONFIG: 'config'        // global system settings
    },

    /**
     * Get data from local storage
     */
    get(tier, key = null) {
        const fullKey = this._getLocalKey(tier, key);
        const saved = localStorage.getItem(fullKey);
        if (!saved) return null;
        try {
            return JSON.parse(saved);
        } catch (e) {
            return saved; // Raw string (source code)
        }
    },

    /**
     * Save data locally and schedule a remote sync
     */
    save(tier, key, data) {
        const fullKey = this._getLocalKey(tier, key);
        const serialized = typeof data === 'string' ? data : JSON.stringify(data);
        localStorage.setItem(fullKey, serialized);

        // Notify sync handler (window._RS_HANDLER is the RemoteStorageHandler instance)
        if (typeof window !== 'undefined' && window._RS_HANDLER) {
            this._triggerRemoteSync(tier, key, data);
        }
    },

    /**
     * Trigger remote sync based on tier
     */
    _triggerRemoteSync(tier, key, data) {
        const handler = window._RS_HANDLER;
        switch(tier) {
            case this.TIERS.OPERATORS:
                handler.pushOperator(key, data.script, data.schema);
                break;
            case this.TIERS.WINDOWS:
            case this.TIERS.DESKTOP:
                // Sync combined layout
                handler.pushLayout({
                    windows: tier === this.TIERS.WINDOWS ? data : this.get(this.TIERS.WINDOWS),
                    desktop: tier === this.TIERS.DESKTOP ? data : this.get(this.TIERS.DESKTOP)
                });
                break;
            case this.TIERS.ASSETS:
                handler.pushAsset(key, data);
                break;
            case this.TIERS.APP_DATA:
            case this.TIERS.CONFIG:
                handler.pushConfig(tier, key, data);
                break;
        }
    },

    /**
     * Internal helper to map tiers to LocalStorage keys
     */
    _getLocalKey(tier, key) {
        if (tier === this.TIERS.OPERATORS) {
            if (key) {
                const prefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
                return `${prefix}_op_${key}`;
            }
            return DYNAMIC_OPS_KEY;
        }
        if (tier === this.TIERS.WINDOWS) return NODE_STATE_KEY;
        if (tier === this.TIERS.DESKTOP) return DESKTOP_STATE_KEY;
        if (tier === this.TIERS.ASSETS) {
            const prefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
            return `${prefix}_asset_${key}`;
        }
        
        const prefix = import.meta.env.VITE_STORAGE_PREFIX || 'demo';
        return `${prefix}_${tier}_${key || 'default'}`;
    },

    /**
     * Merge incoming remote data into local state
     */
    mergeRemote(tier, key, remoteData) {
        // Access blackboard and setActions via window to avoid circular imports
        const bb = window.blackboard;
        if (!bb) {
            console.warn('[Worksheet] Blackboard not initialized yet, skipping merge.');
            return null;
        }

        if (tier === this.TIERS.OPERATORS) {
            const path = `user/${key}`;
            const localOp = bb.dynamicOps()[path];
            const baseOp = shadowOps[path];
            const merged = Merger.mergeOperator(path, localOp, baseOp, remoteData);
            
            bb.publishDynamicOp(path, merged.schema, merged.script, true);
            if (!merged.hasConflicts) syncActions.updateShadowOp(path, remoteData);
            return merged;
        }

        if (tier === this.TIERS.WINDOWS || tier === this.TIERS.DESKTOP) {
            const localWindows = this.get(this.TIERS.WINDOWS) || [];
            const localDesktop = this.get(this.TIERS.DESKTOP) || [];
            const baseLayout = shadowLayout();

            const merged = Merger.mergeLayout(
                { windows: localWindows, desktop: localDesktop },
                baseLayout,
                remoteData
            );

            if (merged.windows) bb.setOpenWindows(merged.windows);
            if (merged.desktop) bb.setDesktopIcons(merged.desktop);
            
            syncActions.updateShadowLayout(remoteData);
            return merged;
        }

        // Default: Last-Write-Wins for non-mergable types (Assets/Config)
        this.save(tier, key, remoteData);
        return remoteData;
    }
};

if (typeof window !== 'undefined') {
    window.Worksheet = Worksheet;
}
