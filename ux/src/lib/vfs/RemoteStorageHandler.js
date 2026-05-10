import RemoteStorage from 'remotestoragejs';
import Widget from 'remotestorage-widget';
import JotStorageModule from './RemoteStorageModule';
import { setSyncStatus, setCloudAccount } from '../state/SyncState';
import { blackboard } from '../blackboard';
import { Worksheet } from './Worksheet';
import { syncActions } from '../state/SyncState';

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
    window._RS_HANDLER = this;

    let widget = null;

    // Check for container immediately, or wait for it to appear
    const attachWidget = () => {
        const container = document.getElementById('rs-widget-container');
        if (container) {
            // Lazy-create the widget ONLY when the container exists
            if (!widget) {
                widget = new Widget(rs, { leaveOpen: false });
            }
            // Only attach if container isn't already hosting the widget
            if (!container.querySelector('#remotestorage-widget')) {
                widget.attach('rs-widget-container');
            }
            return true;
        }
        return false;
    };

    // Robust re-attachment whenever the container appears in the DOM
    const observer = new MutationObserver(() => {
        attachWidget();
    });
    observer.observe(document.body, { childList: true, subtree: true });
    
    // Initial attempt
    attachWidget();

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
          Worksheet.mergeRemote(Worksheet.TIERS.WINDOWS, null, event.newValue);
        } else if (event.relativePath.startsWith('operators/')) {
          const name = event.relativePath.replace('operators/', '');
          Worksheet.mergeRemote(Worksheet.TIERS.OPERATORS, name, event.newValue);
        } else if (event.relativePath.startsWith('assets/')) {
          const id = event.relativePath.replace('assets/', '');
          Worksheet.mergeRemote(Worksheet.TIERS.ASSETS, id, event.newValue);
        } else if (event.relativePath.startsWith('config/')) {
           // Handle app_data or config
           const parts = event.relativePath.split('/');
           const tier = parts[1]; // app_data or config
           const key = parts.slice(2).join('/');
           Worksheet.mergeRemote(tier, key, event.newValue);
        }
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
          Worksheet.mergeRemote(Worksheet.TIERS.OPERATORS, name, remoteOp);
        }
      }

      // 2. Sync Layout
      const remoteLayout = await rs.jotcad.getLayout();
      if (remoteLayout) {
        Worksheet.mergeRemote(Worksheet.TIERS.WINDOWS, null, remoteLayout);
      }

      setSyncStatus('idle');
    } catch (e) {
      console.error('[RemoteStorage] Initial sync failed:', e);
      setSyncStatus('error');
    }
  },

  async pushLayout(layout) {
    if (!rs.connected) return;
    await rs.jotcad.saveLayout(layout);
    syncActions.updateShadowLayout(layout);
  },

  async pushOperator(path, script, schema) {
    if (!rs.connected || !path) return;
    const name = path.replace('user/', '');
    const opData = { script, schema };
    await rs.jotcad.saveOperator(name, opData);
    syncActions.updateShadowOp(path, opData);
  },

  async pushAsset(id, data) {
    if (!rs.connected) return;
    await rs.jotcad.saveAsset(id, data);
  },

  async pushConfig(tier, key, data) {
    if (!rs.connected) return;
    await rs.jotcad.saveConfig(tier, key, data);
  }
};
