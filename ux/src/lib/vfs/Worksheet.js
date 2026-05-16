console.log('[Trace] Executing Worksheet.js');
import { batch } from 'solid-js';
import { reconcile } from 'solid-js/store';
import { shadowOps, shadowLayout, syncActions } from '../state/SyncState';
import { DYNAMIC_OPS_KEY, NODE_STATE_KEY, DESKTOP_STATE_KEY } from '../state/Config';

/**
 * Worksheet: The unified persistence and sync layer.
 * Manages LocalStorage with deferred RemoteStorage synchronization.
 */
export const Worksheet = {
    TIERS: {
        OPERATORS: 'operators', 
        DESKTOP: 'desktop',     
        WINDOWS: 'windows',     
        ASSETS: 'assets',       
        APP_DATA: 'app_data',   
        CONFIG: 'config'        
    },

    _isSyncing: false,

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
            return saved; 
        }
    },

    /**
     * Save data locally and mark the state as dirty.
     * @param {boolean} triggerSync If true, resets idle timer and marks dirty.
     */
    save(tier, key, data, triggerSync = true) {
        const fullKey = this._getLocalKey(tier, key);
        const serialized = typeof data === 'string' ? data : JSON.stringify(data);
        localStorage.setItem(fullKey, serialized);

        // DELIBERATE GUARD: Don't trigger sync activity if we are already in a sync cycle
        if (triggerSync && !this._isSyncing) {
            // Mark as dirty and reset idle timer
            import('./CloudSync').then(({ CloudSync }) => {
                if (CloudSync && CloudSync.activity) {
                    CloudSync.activity();
                }
            });
        }
    },

    _getLocalKey(tier, key) {
        if (tier === this.TIERS.OPERATORS) {
            if (key) {
                return `jot_op_${key}`;
            }
            return DYNAMIC_OPS_KEY;
        }
        if (tier === this.TIERS.WINDOWS) return NODE_STATE_KEY;
        if (tier === this.TIERS.DESKTOP) return DESKTOP_STATE_KEY;
        if (tier === this.TIERS.ASSETS) {
            return `jot_asset_${key}`;
        }
        
        return `jot_${tier}_${key || 'default'}`;
    },

    /**
     * Merge incoming remote data into local state
     */
    async mergeRemote(tier, key, remoteData) {
        const bb = window.blackboard;
        if (!bb) return null;

        const { Merger } = await import('./CloudSync');
        
        this._isSyncing = true;
        console.log(`[Worksheet] mergeRemote started for tier: ${tier}`);

        try {
            if (tier === this.TIERS.OPERATORS) {
                const remoteOps = remoteData || {};
                const localOps = bb.dynamicOps();
                
                batch(() => {
                    for (const [vfsKey, remoteOp] of Object.entries(remoteOps)) {
                        const path = `user/${vfsKey}`;
                        const localOp = localOps[path];
                        const baseOp = JSON.parse(JSON.stringify(shadowOps[path] || {}));

                        const merged = Merger.mergeOperator(path, localOp, baseOp, remoteOp);
                        
                        if (!merged.schema?.arguments) {
                            console.warn('[Worksheet] mergeRemote: skipping malformed op:', path);
                            continue; 
                        }

                        bb.publishDynamicOp(path, merged.schema, merged.script, false);
                        if (!merged.hasConflicts) syncActions.updateShadowOp(path, remoteOp);
                        this.save(tier, vfsKey, merged, false);
                    }
                });
                return remoteOps;
            }

            if (tier === this.TIERS.WINDOWS || tier === this.TIERS.DESKTOP) {
                const localWindows = this.get(this.TIERS.WINDOWS) || [];
                const localDesktop = this.get(this.TIERS.DESKTOP) || [];
                const baseLayout = JSON.parse(JSON.stringify(shadowLayout || { windows: [], desktop: [] }));

                const merged = Merger.mergeLayout(
                    { windows: localWindows, desktop: localDesktop },
                    baseLayout,
                    remoteData
                );

                if (!merged || !merged.windows || !merged.desktop) {
                    console.warn('[Worksheet] Layout merge produced invalid result, aborting merge to prevent data loss.');
                    return null;
                }

                console.log(`[Worksheet] Layout merged. Windows: ${merged.windows.length}, Icons: ${merged.desktop.length}`);

                // ID-Stable reconciliation for Solid stores
                try {
                    if (typeof bb.setOpenWindows === 'function') {
                        bb.setOpenWindows(reconcile(merged.windows, { key: 'id' }));
                        this.save(this.TIERS.WINDOWS, null, merged.windows, false);
                    }
                } catch (err) {
                    console.error('[Worksheet] setOpenWindows reconciliation failed:', err);
                }

                try {
                    if (typeof bb.setDesktopIcons === 'function') {
                        // Ensure system icons are present
                        const repaired = [
                            { id: 'app:catalog', type: 'app', label: 'Catalog', x: 40, y: 40, target: 'catalog', icon: 'Database' },
                            { id: 'app:mesh', type: 'app', label: 'Mesh Graph', x: 140, y: 40, target: 'mesh', icon: 'Network' },
                            { id: 'app:settings', type: 'app', label: 'Settings', x: 40, y: 140, target: 'settings', icon: 'Settings' },
                            { id: 'app:console', type: 'app', label: 'Console', x: 140, y: 140, target: 'console', icon: 'Terminal' },
                            { id: 'action:sync', type: 'action', label: 'Sync Cloud', x: 40, y: 240, target: 'sync_cloud', icon: 'Cloud' },
                            { id: 'action:new', type: 'action', label: 'New Op', x: 140, y: 240, target: 'new_op', icon: 'Plus' }
                        ];

                        const finalDesktop = [...repaired];
                        for (const icon of merged.desktop) {
                            if (icon && icon.id && !finalDesktop.find(ri => ri.id === icon.id)) finalDesktop.push(icon);
                        }

                        bb.setDesktopIcons(reconcile(finalDesktop, { key: 'id' }));
                        this.save(this.TIERS.DESKTOP, null, finalDesktop, false);
                    }
                } catch (err) {
                    console.error('[Worksheet] setDesktopIcons reconciliation failed:', err);
                }                
                syncActions.updateShadowLayout(remoteData);
                return merged;
            }

            this.save(tier, key, remoteData, false);
            return remoteData;
        } finally {
            this._isSyncing = false;
            console.log(`[Worksheet] mergeRemote finished for tier: ${tier}`);
        }
    }
};

if (typeof window !== 'undefined') {
    window.Worksheet = Worksheet;
}
