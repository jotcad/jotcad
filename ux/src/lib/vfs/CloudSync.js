import { merge as diff3Merge } from 'node-diff3';
import { combineMergers, trimergeJsonDeepEqual, trimergeJsonObject } from 'trimerge';
import { shadowOps, shadowLayout, syncActions, setSyncStatus, syncStatus } from '../state/SyncState';
import { dynamicOps, openEditors, editorActions } from '../state/AppState';
import { blackboard } from '../blackboard';
import RemoteStorage from 'remotestoragejs';
import './RemoteStorageModule';

const jsonMerger = combineMergers(trimergeJsonDeepEqual, trimergeJsonObject);

/**
 * 3-Way Merge Engine
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
 * Real RemoteStorage Provider
 */
export const RemoteStorageProvider = {
    _getRS() {
        const rs = new RemoteStorage({ modules: ['jotcad'] });
        rs.access.claim('jotcad', 'rw');
        return rs;
    },
    async list() {
        const rs = this._getRS();
        const list = await rs.jotcad.getOperators();
        return list ? Object.keys(list) : [];
    },
    async get(name) {
        const rs = this._getRS();
        return await rs.jotcad.getOperator(name);
    },
    async put(name, data) {
        const rs = this._getRS();
        await rs.jotcad.saveOperator(name, data);
    }
};

/**
 * High-level Sync Orchestrator
 */
export const CloudSync = {
    provider: RemoteStorageProvider,

    async syncAll() {
        setSyncStatus('syncing');
        try {
            const remotePaths = await this.provider.list();
            const localOps = blackboard.dynamicOps();
            
            // 1. Pull / Merge Operators
            for (const name of remotePaths) {
                const path = `user/${name}`;
                const remoteOp = await this.provider.get(name);
                const localOp = localOps[path];
                const baseOp = shadowOps[path];

                if (!localOp) {
                    // New operator from cloud
                    blackboard.publishDynamicOp(path, remoteOp.schema, remoteOp.script, true);
                    syncActions.updateShadowOp(path, remoteOp);
                } else if (JSON.stringify(localOp) !== JSON.stringify(remoteOp)) {
                    // Potential conflict or update
                    const merged = Merger.mergeOperator(path, localOp, baseOp, remoteOp);
                    
                    blackboard.publishDynamicOp(path, merged.schema, merged.script, true);
                    if (merged.hasConflicts) {
                        setSyncStatus('conflict');
                    }
                    
                    if (!merged.hasConflicts) {
                        syncActions.updateShadowOp(path, remoteOp);
                    }
                }
            }

            // 2. Push Local-only Operators
            for (const [path, localOp] of Object.entries(localOps)) {
                const name = path.replace('user/', '');
                const isRemote = remotePaths.includes(name);
                if (!isRemote) {
                    await this.provider.put(name, localOp);
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
