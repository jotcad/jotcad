import { merge as diff3Merge } from 'node-diff3';
import { combineMergers, trimergeJsonDeepEqual, trimergeJsonObject } from 'trimerge';

const jsonMerger = combineMergers(trimergeJsonDeepEqual, trimergeJsonObject);

/**
 * 3-Way Merge Engine - Pure Logic
 * NOTE: Do not import AppState or blackboard here to avoid circularity.
 */
export const Merger = {
    /**
     * Merges JOT Operator Source Code
     */
    mergeOperator(path, localOp, baseOp, remoteOp) {
        console.log(`[Merger] Merging Operator: ${path}`);
        
        // 1. Merge the Script (Text)
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

        // 2. Merge the Schema (JSON) using trimerge
        const schema = jsonMerger(baseOp?.schema || {}, localOp?.schema || {}, remoteOp?.schema || {});

        return {
            script,
            schema,
            hasConflicts
        };
    },

    /**
     * Merges Blackboard Layout / Editor State
     */
    mergeLayout(localLayout, baseLayout, remoteLayout) {
        console.log(`[Merger] Merging Layout State`);
        return jsonMerger(baseLayout || {}, localLayout || {}, remoteLayout || {});
    }
};

/**
 * Sync Orchestrator
 * Uses runtime references to blackboard to avoid circular imports.
 */
export const CloudSync = {
    async syncAll() {
        const bb = window.blackboard;
        if (!bb) return;

        const { shadowOps, syncActions, setSyncStatus, syncStatus } = await import('../state/SyncState');
        
        setSyncStatus('syncing');
        try {
            const handler = window._RS_HANDLER;
            if (!handler) throw new Error('RemoteStorageHandler not initialized');

            // 1. Pull / Merge Operators
            const operatorsListing = await handler.getOperators();
            const localOps = bb.dynamicOps();
            
            if (operatorsListing) {
                for (const name of Object.keys(operatorsListing)) {
                    const path = `user/${name}`;
                    const remoteOp = await handler.getOperator(name);
                    const localOp = localOps[path];
                    const baseOp = shadowOps[path];

                    if (!localOp) {
                        bb.publishDynamicOp(path, remoteOp.schema, remoteOp.script, true);
                        syncActions.updateShadowOp(path, remoteOp);
                    } else if (JSON.stringify(localOp) !== JSON.stringify(remoteOp)) {
                        const merged = Merger.mergeOperator(path, localOp, baseOp, remoteOp);
                        bb.publishDynamicOp(path, merged.schema, merged.script, true);
                        if (merged.hasConflicts) setSyncStatus('conflict');
                        if (!merged.hasConflicts) syncActions.updateShadowOp(path, remoteOp);
                    }
                }
            }

            // 2. Push Local-only Operators
            for (const [path, localOp] of Object.entries(localOps)) {
                const name = path.replace('user/', '');
                const isRemote = operatorsListing && operatorsListing[name];
                if (!isRemote) {
                    await handler.pushOperator(path, localOp.script, localOp.schema);
                    syncActions.updateShadowOp(path, localOp);
                }
            }

            if (syncStatus() === 'syncing') setSyncStatus('idle');
        } catch (err) {
            console.error('[CloudSync] Sync failed:', err);
            setSyncStatus('error');
        }
    }
};
