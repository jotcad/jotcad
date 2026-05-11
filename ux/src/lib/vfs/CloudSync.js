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

        if (typeof left === 'object' && left !== null && typeof right === 'object' && right !== null) {
            if (Array.isArray(left) && Array.isArray(right)) {
                // Array merge: ID-aware or fallback to local
                const origArr = Array.isArray(orig) ? orig : [];
                const origMap = new Map(origArr.filter(i => i?.id).map(i => [i.id, i]));
                const leftMap = new Map(left.filter(i => i?.id).map(i => [i.id, i]));
                const rightMap = new Map(right.filter(i => i?.id).map(i => [i.id, i]));

                if (leftMap.size === 0 && rightMap.size === 0) return left; // Fallback for simple arrays

                const allIds = new Set([...leftMap.keys(), ...rightMap.keys()]);
                const result = [];

                for (const id of allIds) {
                    const o = origMap.get(id);
                    const l = leftMap.get(id);
                    const r = rightMap.get(id);
                    
                    if (l === undefined) {
                        // Remote add or local delete
                        if (o === undefined || JSON.stringify(o) !== JSON.stringify(r)) {
                            result.push(r);
                        }
                        continue;
                    }
                    if (r === undefined) {
                        // Local add or remote delete
                        if (o === undefined || JSON.stringify(o) !== JSON.stringify(l)) {
                            result.push(l);
                        }
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
        }
        return left; // Conflict fallback
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
            
            return this.mergeJson(base, local, remote);
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
                console.log('[CloudSync] 60s Idle reached. Triggering auto-sync.');
                this.syncAll();
            }
        }, IDLE_TIME);
    },

    async syncAll() {
        const bb = window.blackboard;
        if (!bb) return;
        if (syncStatus() === 'syncing') return;

        setSyncStatus('syncing');
        console.group('[CloudSync] Starting Synchronization cycle');

        try {
            const handler = window._RS_HANDLER;
            if (!handler) throw new Error('RemoteStorageHandler not initialized');

            // --- 1. SYNC OPERATORS ---
            console.log('[CloudSync] Step 1: Synchronizing Operators');
            const operatorsListing = (await handler.getOperators()) || {};
            const localOps = bb.dynamicOps();
            
            for (const name of Object.keys(operatorsListing)) {
                const path = `user/${name}`;
                const remoteOp = await handler.getOperator(name);
                const localOp = localOps[path];
                const baseOp = JSON.parse(JSON.stringify(shadowOps[path] || null));

                if (!localOp) {
                    if (baseOp && JSON.stringify(baseOp) === JSON.stringify(remoteOp)) {
                        console.log(`[CloudSync] Operator deleted locally, removing from remote: ${name}`);
                        await handler.removeOperator(path);
                    } else {
                        console.log(`[CloudSync] New operator on remote, pulling: ${name}`);
                        bb.publishDynamicOp(path, remoteOp.schema, remoteOp.script, false);
                        syncActions.updateShadowOp(path, remoteOp);
                    }
                } else {
                    const localStr = JSON.stringify({ script: localOp.script, schema: localOp.schema });
                    const remoteStr = JSON.stringify({ script: remoteOp.script, schema: remoteOp.schema });
                    
                    if (localStr !== remoteStr) {
                        console.log(`[CloudSync] Operator modified on both sides, merging: ${name}`);
                        const merged = Merger.mergeOperator(path, localOp, baseOp || {}, remoteOp);
                        bb.publishDynamicOp(path, merged.schema, merged.script, false);
                        if (merged.hasConflicts) setSyncStatus('conflict');
                        syncActions.updateShadowOp(path, remoteOp);
                    }
                }
            }

            // Push local-only ops
            for (const [path, localOp] of Object.entries(localOps)) {
                const name = path.replace('user/', '');
                const isRemote = operatorsListing[name] || operatorsListing[name + '/'];
                if (!isRemote) {
                    const baseOp = shadowOps[path];
                    if (baseOp) {
                        console.log(`[CloudSync] Operator deleted on remote, removing locally: ${path}`);
                        bb.removeDynamicOp(path);
                        syncActions.removeShadowOp(path);
                    } else {
                        console.log(`[CloudSync] New operator locally, pushing: ${path}`);
                        await handler.pushOperator(path, localOp.script, localOp.schema);
                        syncActions.updateShadowOp(path, localOp);
                    }
                }
            }

            // Persist operators to disk silently
            const ws = (await import('./Worksheet')).Worksheet;
            ws.save('operators', null, bb.dynamicOps(), false);

            // --- 2. SYNC LAYOUT ---
            console.log('[CloudSync] Step 2: Synchronizing Layout');
            const remoteLayout = await handler.getLayout();
            const baseLayout = JSON.parse(JSON.stringify(shadowLayout));

            // Use blackboard stores directly as Live Local Source of Truth
            const localLayout = { 
                windows: JSON.parse(JSON.stringify(bb.openWindows())), 
                desktop: JSON.parse(JSON.stringify(bb.desktopIcons())) 
            };

            const localLayoutStr = JSON.stringify(localLayout);
            const remoteLayoutStr = JSON.stringify(remoteLayout || { windows: [], desktop: [] });

            if (localLayoutStr !== remoteLayoutStr) {
                if (!remoteLayout || (remoteLayout.windows?.length === 0 && remoteLayout.desktop?.length === 0)) {
                    if (localLayout.windows.length > 0 || localLayout.desktop.length > 0) {
                        console.log('[CloudSync] Remote layout is empty, pushing local state.');
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
            console.groupEnd();
        } catch (err) {
            console.error('[CloudSync] Sync failed:', err);
            console.groupEnd();
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
