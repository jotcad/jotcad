import RemoteStorage from 'remotestoragejs';
import Widget from 'remotestorage-widget';
import JotStorageModule from './RemoteStorageModule';
import { setSyncStatus, setCloudAccount } from '../state/SyncState';
import { blackboard } from '../blackboard';
import { Merger } from './CloudSync';
import { shadowOps, syncActions, shadowLayout } from '../state/SyncState';
import { createEffect } from 'solid-js';

const rs = new RemoteStorage({
  cache: true,
  logging: true
});

// Enable Google Drive & Dropbox as backends
rs.setApiKeys({
  googledrive: import.meta.env.VITE_GOOGLE_CLIENT_ID,
  dropbox: import.meta.env.VITE_DROPBOX_APP_KEY
});

rs.addModule(JotStorageModule);
rs.access.claim('jotcad', 'rw');

/**
 * RemoteStorage Lifecycle Handler
 */
export const RemoteStorageHandler = {
  init() {
    if (typeof window === 'undefined') return;

    // Initialize Widget
    const widget = new Widget(rs);
    widget.attach('rs-widget-container');

    rs.on('connected', () => {
      const userAddress = rs.remote.userAddress;
      console.log(`[RemoteStorage] Connected: ${userAddress}`);
      setCloudAccount({ email: userAddress, name: userAddress.split('@')[0] });
      setSyncStatus('idle');
      this.initialSync();
    });

    rs.on('network-offline', () => {
      setSyncStatus('error');
    });

    rs.on('network-online', () => {
      setSyncStatus('idle');
    });

    // Handle incoming changes from other devices
    rs.jotcad.onChange((event) => {
      if (event.origin === 'remote') {
        if (event.relativePath === 'layout') {
          this.handleRemoteLayoutChange(event.newValue);
        } else if (event.relativePath.startsWith('operators/')) {
          this.handleRemoteChange(event);
        }
      }
    });

    // --- DECIPHERING THE BLACKBOARD BUS ---
    // This is how we keep the VFS layer clean.
    createEffect(() => {
        const ev = blackboard.events();
        if (ev.type === 'op:publish' || ev.type === 'op:update') {
            this.pushOperator(ev.data.path || ev.data.id, ev.data.script, ev.data.schema);
        }
        if (ev.type === 'editor:open' || ev.type === 'editor:close') {
            const editors = blackboard.openEditors();
            this.pushLayout({ editors });
        }
    });
  },

  async initialSync() {
    setSyncStatus('syncing');
    try {
      // 1. Sync Operators
      const operators = await rs.jotcad.getOperators();
      if (operators) {
        for (const name of Object.keys(operators)) {
          const remoteOp = await rs.jotcad.getOperator(name);
          this.syncOpWithLocal(name, remoteOp);
        }
      }

      // 2. Sync Layout
      const remoteLayout = await rs.jotcad.getLayout();
      if (remoteLayout) {
        this.handleRemoteLayoutChange(remoteLayout);
      }

      setSyncStatus('idle');
    } catch (e) {
      console.error('[RemoteStorage] Initial sync failed:', e);
      setSyncStatus('error');
    }
  },

  handleRemoteLayoutChange(remoteLayout) {
    console.log('[RemoteStorage] Remote layout change:', remoteLayout);
    const localLayout = { 
        editors: JSON.parse(localStorage.getItem(blackboard.NODE_STATE_KEY) || '[]') 
    };
    const baseLayout = shadowLayout();

    const merged = Merger.mergeLayout(localLayout, baseLayout, remoteLayout);
    if (merged && merged.editors) {
       blackboard.setOpenEditors(merged.editors);
       syncActions.updateShadowLayout(remoteLayout);
    }
  },

  async pushLayout(layout) {
    if (!rs.connected) return;
    await rs.jotcad.saveLayout(layout);
    syncActions.updateShadowLayout(layout);
  },

  async syncOpWithLocal(name, remoteOp) {
    const path = `user/${name}`;
    const localOps = blackboard.dynamicOps();
    const localOp = localOps[path];
    const baseOp = shadowOps[path];

    if (!localOp) {
      blackboard.publishDynamicOp(path, remoteOp.schema, remoteOp.script, true);
      syncActions.updateShadowOp(path, remoteOp);
    } else if (JSON.stringify(localOp) !== JSON.stringify(remoteOp)) {
      const merged = Merger.mergeOperator(path, localOp, baseOp, remoteOp);
      blackboard.publishDynamicOp(path, merged.schema, merged.script, true);
      
      if (merged.hasConflicts) {
        setSyncStatus('conflict');
      } else {
        syncActions.updateShadowOp(path, remoteOp);
      }
    }
  },

  handleRemoteChange(event) {
    console.log('[RemoteStorage] Remote change detected:', event);
    if (event.relativePath.startsWith('operators/')) {
      const name = event.relativePath.replace('operators/', '');
      this.syncOpWithLocal(name, event.newValue);
    }
  },

  async pushOperator(path, script, schema) {
    if (!rs.connected || !path) return;
    const name = path.replace('user/', '');
    const opData = { script, schema };
    await rs.jotcad.saveOperator(name, opData);
    syncActions.updateShadowOp(path, opData);
  }
};
