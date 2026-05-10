import { merge as diff3Merge } from 'node-diff3';
import { combineMergers, trimergeJsonDeepEqual, trimergeJsonObject } from 'trimerge';
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
 * Custom Merger for Arrays of Objects with IDs.
 * This prevents the "CannotMergeError" when windows or desktop icons differ.
 */
const trimergeArrayById = (orig, left, right, merge) => {
    if (!Array.isArray(left) || !Array.isArray(right)) return undefined;

    const origMap = new Map((Array.isArray(orig) ? orig : []).map(i => [i.id, i]));
    const leftMap = new Map(left.map(i => [i.id, i]));
    const rightMap = new Map(right.map(i => [i.id, i]));

    const allIds = new Set([...leftMap.keys(), ...rightMap.keys()]);
    const result = [];

    for (const id of allIds) {
        const o = origMap.get(id);
        const l = leftMap.get(id);
        const r = rightMap.get(id);

        if (l === undefined) {
            // New in remote or modified in remote while deleted in local.
            if (o === undefined || JSON.stringify(o) !== JSON.stringify(r)) {
                result.push(r);
            }
            continue;
        }
        if (r === undefined) {
            // New in local or modified locally while deleted in remote.
            if (o === undefined || JSON.stringify(o) !== JSON.stringify(l)) {
                result.push(l);
            }
            continue;
        }

        // Both exist: perform a deep merge on the object itself
        result.push(merge(o, l, r));
    }
    return result;
};

const jsonMerger = combineMergers(trimergeJsonDeepEqual, trimergeArrayById, trimergeJsonObject);

/**
 * 3-Way Merge Engine - Pure Logic
 */
export const Merger = {
    mergeOperator(path, localOp, baseOp, remoteOp) {
        console.log(`[Merger] Merging Operator: ${path}`);
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

        const schema = jsonMerger(baseOp?.schema || {}, localOp?.schema || {}, remoteOp?.schema || {});

        return { script, schema, hasConflicts };
    },

    mergeLayout(localLayout, baseLayout, remoteLayout) {
        console.log(`[Merger] Performing Layout Merge`);
        try {
            const base = {
                windows: Array.isArray(baseLayout?.windows) ? baseLayout.windows : [],
                desktop: Array.isArray(baseLayout?.desktop) ? baseLayout.desktop : []
            };
            const res = jsonMerger(base, localLayout || {}, remoteLayout || {});
            return res || localLayout;
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
const IDLE_TIME = 60000; // 60 seconds

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
        try {
            const handler = window._RS_HANDLER;
            if (!handler) throw new Error('RemoteStorageHandler not initialized');

            // --- 1. SYNC OPERATORS ---
            const operatorsListing = await handler.getOperators();
            const localOps = bb.dynamicOps();
            
            if (operatorsListing) {
                for (const name of Object.keys(operatorsListing)) {
                    const path = `user/${name}`;
                    const remoteOp = await handler.getOperator(name);
                    const localOp = localOps[path];
                    const baseOp = JSON.parse(JSON.stringify(shadowOps[path] || {}));

                    if (!localOp) {
                        bb.publishDynamicOp(path, remoteOp.schema, remoteOp.script, true);
                        syncActions.updateShadowOp(path, remoteOp);
                    } else {
                        const localStr = JSON.stringify({ script: localOp.script, schema: localOp.schema });
                        const remoteStr = JSON.stringify({ script: remoteOp.script, schema: remoteOp.schema });
                        
                        if (localStr !== remoteStr) {
                            const merged = Merger.mergeOperator(path, localOp, baseOp, remoteOp);
                            bb.publishDynamicOp(path, merged.schema, merged.script, true);
                            if (merged.hasConflicts) setSyncStatus('conflict');
                            syncActions.updateShadowOp(path, remoteOp);
                        }
                    }
                }
            }

            for (const [path, localOp] of Object.entries(localOps)) {
                const name = path.replace('user/', '');
                const isRemote = operatorsListing && operatorsListing[name];
                if (!isRemote) {
                    await handler.pushOperator(path, localOp.script, localOp.schema);
                    syncActions.updateShadowOp(path, localOp);
                }
            }

            // --- 2. SYNC LAYOUT ---
            const remoteLayout = await handler.getLayout();
            
            // Critical: shadowLayout is a Solid proxy, we must serialize to raw JS
            const baseLayout = JSON.parse(JSON.stringify(shadowLayout));

            // Use blackboard stores directly as Live Local Source of Truth
            const localWindows = JSON.parse(JSON.stringify(bb.openWindows()));
            const localDesktop = JSON.parse(JSON.stringify(bb.desktopIcons()));
            const localLayout = { windows: localWindows, desktop: localDesktop };

            console.log('[CloudSync] Starting layout sync. Local Windows:', localWindows.length, 'Remote Windows:', remoteLayout?.windows?.length || 0);

            const localLayoutStr = JSON.stringify(localLayout);
            const remoteLayoutStr = JSON.stringify(remoteLayout || { windows: [], desktop: [] });

            if (localLayoutStr !== remoteLayoutStr) {
                if (!remoteLayout || (remoteLayout.windows?.length === 0 && remoteLayout.desktop?.length === 0)) {
                    // First time sync or empty remote: Push local to remote
                    if (localLayout.windows.length > 0 || localLayout.desktop.length > 0) {
                        console.log('[CloudSync] Remote is empty, pushing local state.');
                        await handler.pushLayout(localLayout);
                        syncActions.updateShadowLayout(localLayout);
                    }
                } else {
                    const merged = Merger.mergeLayout(localLayout, baseLayout, remoteLayout);
                    console.log('[CloudSync] Merge completed. Windows after merge:', merged.windows?.length);
                    
                    const { reconcile } = await import('solid-js/store');
                    // ID-Stable reconciliation for Solid stores
                    if (merged.windows) bb.setOpenWindows(reconcile(merged.windows, { key: 'id' }));
                    if (merged.desktop) bb.setDesktopIcons(reconcile(merged.desktop, { key: 'id' }));
                    
                    // Persist to local storage via Worksheet silently
                    const { Worksheet } = await import('./Worksheet');
                    Worksheet.save('windows', null, merged.windows, false);
                    Worksheet.save('desktop', null, merged.desktop, false);

                    await handler.pushLayout(merged);
                    syncActions.updateShadowLayout(merged); 
                }
            }

            setIsDirty(false);
            setLastSyncTime(Date.now());
            if (syncStatus() === 'syncing') setSyncStatus('idle');
        } catch (err) {
            console.error('[CloudSync] Sync failed:', err);
            setSyncStatus('error');
            alert(`Cloud Sync Failed:\n${err.message || err}`);
        }
    }
};
