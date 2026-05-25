// console.log('[Trace] Executing CloudSync.js');
import { merge as diff3Merge } from 'node-diff3';
import { 
    shadowOps, 
    shadowLayout, 
    syncActions, 
    setSyncStatus, 
    syncStatus,
    setIsDirty,
    isDirty,
    setLastSyncTime
} from '../state/SyncState';

/**
 * 3-Way Merge Engine
 */
export const Merger = {
    mergeJson(orig, left, right) {
        if (left === right) return left;
        if (left === orig) return right;
        if (right === orig) return left;

        // NULL SAFETY: If either side is not an object, return the newest or fall back to left
        if (!left || typeof left !== 'object' || !right || typeof right !== 'object') {
            return right !== undefined ? right : left;
        }

        if (Array.isArray(left) && Array.isArray(right)) {
            // Array merge: ID-aware
            const origArr = Array.isArray(orig) ? orig : [];
            const origMap = new Map(origArr.filter(i => i?.id).map(i => [i.id, i]));
            const leftMap = new Map(left.filter(i => i?.id).map(i => [i.id, i]));
            const rightMap = new Map(right.filter(i => i?.id).map(i => [i.id, i]));

            if (leftMap.size === 0 && rightMap.size === 0) return left;

            const allIds = new Set([...leftMap.keys(), ...rightMap.keys()]);
            const result = [];

            for (const id of allIds) {
                const o = origMap.get(id);
                const l = leftMap.get(id);
                const r = rightMap.get(id);
                
                if (l === undefined) {
                    if (o === undefined || JSON.stringify(o) !== JSON.stringify(r)) result.push(r);
                    continue;
                }
                if (r === undefined) {
                    if (o === undefined || JSON.stringify(o) !== JSON.stringify(l)) result.push(l);
                    continue;
                }
                result.push(this.mergeJson(o, l, r));
            }
            return result;
        } else if (!Array.isArray(left) && !Array.isArray(right)) {
            // Object merge
            const allKeys = new Set([
                ...Object.keys(orig || {}),
                ...Object.keys(left),
                ...Object.keys(right)
            ]);
            const res = {};
            for (const key of allKeys) {
                res[key] = this.mergeJson(orig?.[key], left[key], right[key]);
            }
            return res;
        }
        return right || left; 
    },

    mergeOperator(path, localOp, baseOp, remoteOp) {
        const localLines = (localOp?.script || '').split('\n');
        const baseLines = (baseOp?.script || '').split('\n');
        const remoteLines = (remoteOp?.script || '').split('\n');

        const mergeResult = diff3Merge(localLines, baseLines, remoteLines);
        let script = mergeResult.result.join('\n');
        const hasConflicts = mergeResult.conflict;

        if (hasConflicts) {
            script = script.replace(/^<<<<<<<$/gm, '<<<<<<< LOCAL')
                           .replace(/^>>>>>>>$/gm, '>>>>>>> REMOTE');
        }

        const schema = this.mergeJson(baseOp?.schema || {}, localOp?.schema || {}, remoteOp?.schema || {});

        return { script, schema, hasConflicts };
    },

    mergeLayout(localLayout, baseLayout, remoteLayout) {
        try {
            const base = {
                windows: Array.isArray(baseLayout?.windows) ? baseLayout.windows : [],
                desktop: Array.isArray(baseLayout?.desktop) ? baseLayout.desktop : []
            };
            const local = {
                windows: Array.isArray(localLayout?.windows) ? localLayout.windows : [],
                desktop: Array.isArray(localLayout?.desktop) ? localLayout.desktop : []
            };
            const remote = {
                windows: Array.isArray(remoteLayout?.windows) ? remoteLayout.windows : [],
                desktop: Array.isArray(remoteLayout?.desktop) ? remoteLayout.desktop : []
            };
            
            const merged = this.mergeJson(base, local, remote);
            
            // FINAL SAFETY: Ensure result is an object with arrays
            return {
                windows: Array.isArray(merged?.windows) ? merged.windows : local.windows,
                desktop: Array.isArray(merged?.desktop) ? merged.desktop : local.desktop
            };
        } catch (e) {
            console.error('[Merger] Layout merge failed, falling back to local.', e);
            return localLayout;
        }
    }
};

/**
 * Sync Orchestrator
 */
let idleTimer = null;
const IDLE_TIME = 60000; 

export const CloudSync = {
    activity() {
        syncActions.markDirty();
        if (idleTimer) clearTimeout(idleTimer);
        idleTimer = setTimeout(() => {
            if (isDirty()) {
                // console.log('[CloudSync] 60s Idle reached. Triggering auto-sync.');
                this.syncAll();
            }
        }, IDLE_TIME);
    },

    async syncAll() {
        const bb = window.blackboard;
        if (!bb) return;
        if (syncStatus() === 'syncing') return;

        setSyncStatus('syncing');
        // console.group('[CloudSync] Starting Synchronization cycle');

        try {
            const handler = window._RS_HANDLER;
            if (!handler) throw new Error('RemoteStorageHandler not initialized');

            // --- 1. SYNC USER OPS (Unified Catalog) ---
            // console.log('[CloudSync] Step 1: Synchronizing UserOps catalog');
            
            // Fetch with safety timeout
            const fetchTimeout = new Promise((_, reject) => setTimeout(() => reject(new Error('RemoteStorage timeout after 15s')), 15000));
            const remoteUserOps = await Promise.race([
                handler.getUserOps(),
                fetchTimeout
            ]) || {};
            
            const localOps = bb.dynamicOps();
            
            let catalogChanged = false;
            const finalRemoteCatalog = { ...remoteUserOps };
            const processedKeys = new Set();

            // A. Process Remote Catalog (Additions & Updates)
            for (const [vfsKey, remoteOp] of Object.entries(remoteUserOps)) {
                const path = `user/${vfsKey}`;
                processedKeys.add(path);
                
                const localOp = localOps[path];
                const baseOp = shadowOps[path];

                if (!localOp) {
                    // It's on remote, but not in local blackboard
                    if (baseOp) {
                        // DELIBERATE LOCAL DELETION: It was here (in shadow), now it's gone.
                        console.log(`[CloudSync] Local deletion detected for: ${vfsKey}. Removing from remote.`);
                        delete finalRemoteCatalog[vfsKey];
                        syncActions.removeShadowOp(path);
                        catalogChanged = true;
                    } else {
                        // REMOTE ADDITION: It's new to us.
                        console.log(`[CloudSync] New operator on remote, pulling: ${vfsKey}`);
                        if (!remoteOp.schema?.arguments) {
                            console.warn(`[CloudSync] Skipping invalid remote operator '${vfsKey}'.`);
                            continue;
                        }
                        // HYDRATION: persist=false keeps the version number and stops save-loops
                        bb.publishDynamicOp(path, remoteOp.schema, remoteOp.script, false);
                        syncActions.updateShadowOp(path, remoteOp);
                    }
                } else {
                    // It's in both.
                    const localStr = JSON.stringify({ script: localOp.script, schema: localOp.schema });
                    const remoteStr = JSON.stringify({ script: remoteOp.script, schema: remoteOp.schema });
                    
                    if (localStr !== remoteStr) {
                        const baseOpJson = JSON.parse(JSON.stringify(baseOp || null));
                        if (JSON.stringify(remoteOp) === JSON.stringify(baseOpJson)) {
                            // LOCAL UPDATE: Local changed, Remote matches shadow.
                            console.log(`[CloudSync] Local update for: ${vfsKey}. Pushing.`);
                            finalRemoteCatalog[vfsKey] = { script: localOp.script, schema: localOp.schema };
                            syncActions.updateShadowOp(path, localOp);
                            catalogChanged = true;
                        } else {
                            // CONFLICT / REMOTE UPDATE: Remote changed from shadow.
                            console.log(`[CloudSync] Conflict/Remote update for: ${vfsKey}. Merging.`);
                            const merged = Merger.mergeOperator(path, localOp, baseOpJson || {}, remoteOp);
                            // HYDRATION: persist=false
                            bb.publishDynamicOp(path, merged.schema, merged.script, false);
                            if (merged.hasConflicts) setSyncStatus('conflict');
                            syncActions.updateShadowOp(path, remoteOp);
                        }
                    }
                }
            }

            // B. Process Local-Only Additions
            for (const [path, localOp] of Object.entries(localOps)) {
                if (!processedKeys.has(path)) {
                    const vfsKey = path.replace('user/', '');
                    const baseOp = shadowOps[path];
                    
                    if (baseOp) {
                        // REMOTE DELETION: Was in shadow, now gone from remote.
                        console.log(`[CloudSync] Remote deletion detected for: ${path}.`);
                        bb.removeDynamicOp(path);
                        syncActions.removeShadowOp(path);
                    } else {
                        // NEW LOCAL ADDITION
                        console.log(`[CloudSync] New local addition: ${vfsKey}`);
                        finalRemoteCatalog[vfsKey] = { script: localOp.script, schema: localOp.schema };
                        syncActions.updateShadowOp(path, localOp);
                        catalogChanged = true;
                    }
                }
            }

            if (catalogChanged) {
                // console.log('[CloudSync] Pushing updated catalog.');
                await handler.saveUserOps(finalRemoteCatalog);
            }

            // Persist locally silently (triggerSync=false)
            const ws = (await import('./Worksheet')).Worksheet;
            ws.save('operators', null, bb.dynamicOps(), false);

            // --- 2. SYNC LAYOUT ---
            // console.log('[CloudSync] Step 2: Synchronizing Layout');
            const remoteLayout = await handler.getLayout();
            const baseLayout = JSON.parse(JSON.stringify(shadowLayout));

            const localLayout = { 
                windows: JSON.parse(JSON.stringify(bb.openWindows())), 
                desktop: JSON.parse(JSON.stringify(bb.desktopIcons())) 
            };

            if (JSON.stringify(localLayout) !== JSON.stringify(remoteLayout || { windows: [], desktop: [] })) {
                if (!remoteLayout || (remoteLayout.windows?.length === 0 && remoteLayout.desktop?.length === 0)) {
                    if (localLayout.windows.length > 0 || localLayout.desktop.length > 0) {
                        await handler.pushLayout(localLayout);
                        syncActions.updateShadowLayout(localLayout);
                    }
                } else {
                    const merged = Merger.mergeLayout(localLayout, baseLayout, remoteLayout);
                    const { reconcile } = await import('solid-js/store');
                    if (merged.windows) bb.setOpenWindows(reconcile(merged.windows, { key: 'id' }));
                    if (merged.desktop) bb.setDesktopIcons(reconcile(merged.desktop, { key: 'id' }));
                    
                    ws.save('windows', null, merged.windows, false);
                    ws.save('desktop', null, merged.desktop, false);

                    await handler.pushLayout(merged);
                    syncActions.updateShadowLayout(merged); 
                }
            }

            setIsDirty(false);
            setLastSyncTime(Date.now());
            if (syncStatus() === 'syncing') setSyncStatus('idle');
            // console.groupEnd();
        } catch (err) {
            console.error('[CloudSync] Sync failed:', err);
            // console.groupEnd();
            setSyncStatus('error');
            alert(`Cloud Sync Failed:\n${err.message || err}`);
        }
    }
};

// Hook into Worksheet for activity tracking
import { Worksheet } from './Worksheet';
const originalSave = Worksheet.save.bind(Worksheet);
Worksheet.save = (tier, key, data, triggerSync = true) => {
    originalSave(tier, key, data, triggerSync);
    if (triggerSync) CloudSync.activity();
};
