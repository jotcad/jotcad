import { setSyncStatus, setCloudAccount } from '../state/SyncState';
import { Worksheet } from './Worksheet';
import { syncActions } from '../state/SyncState';
import RemoteStorage from 'remotestoragejs';
import Widget from 'remotestorage-widget';
import JotStorageModule from './RemoteStorageModule.js';

let rs = null;

export const RemoteStorageHandler = {
  init() {
    if (typeof window === 'undefined' || rs) return;

    console.log('[RemoteStorageHandler] Initializing...');

    rs = new RemoteStorage({
      cache: true,
      logging: true
    });

    // --- CRITICAL LIBRARY PATCH ---
    // Fixes the 'stripQuotes' TypeError in GoogleDrive/Dropbox wire clients
    // caused by lost 'this' context in callbacks.
    rs.on('features-loaded', () => {
        if (rs.remote) {
            console.log('[RemoteStorageHandler] Patching WireClient scope...');
            // Bind stripQuotes to the remote instance so it survives callbacks
            if (typeof rs.remote.stripQuotes === 'function') {
                rs.remote.stripQuotes = rs.remote.stripQuotes.bind(rs.remote);
            }
            // Add a fallback just in case the method itself is missing on some prototypes
            if (!rs.remote.stripQuotes) {
                rs.remote.stripQuotes = (str) => (typeof str === 'string' ? str.replace(/["']/g, '') : str);
            }
        }
    });

    rs.setApiKeys({
      googledrive: '594109471805-4a27m37mlrasmjoh0cap2g97dkgnc2p4.apps.googleusercontent.com',
      dropbox: import.meta.env.VITE_DROPBOX_APP_KEY
    });

    rs.addModule(JotStorageModule);
    rs.access.claim('jotcad', 'rw');

    window._RS_HANDLER = this;

    const widget = new Widget(rs, { leaveOpen: false });
    
    // Simple re-attach logic
    const interval = setInterval(() => {
        const container = document.getElementById('rs-widget-container');
        if (container && !container.querySelector('.rs-widget')) {
            widget.attach('rs-widget-container');
        }
    }, 1000);

    rs.on('connected', async () => {
      const userAddress = rs.remote.userAddress;
      console.log(`%c[RemoteStorage] Connected: ${userAddress}`, 'color: #00ffff; font-weight: bold;');
      setCloudAccount({ email: userAddress, name: userAddress.split('@')[0] });
      setSyncStatus('idle');

      // Small delay to allow the app to finish its first paint
      await new Promise(r => setTimeout(r, 2000));
      this.initialSync();
    });

    rs.on('network-offline', () => setSyncStatus('error'));
    rs.on('network-online', () => setSyncStatus('idle'));

    rs.jotcad.onChange(async (event) => {
      if (event.origin === 'remote') {
        console.log(`[RemoteStorage] Remote change: ${event.relativePath}`);
        if (event.relativePath === 'layout') {
          await Worksheet.mergeRemote(Worksheet.TIERS.WINDOWS, null, event.newValue);
        } else if (event.relativePath === 'userops') {
          await Worksheet.mergeRemote(Worksheet.TIERS.OPERATORS, null, event.newValue);
        }
      }
    });
  },

  async initialSync() {
    if (!rs) return;
    console.log('[RemoteStorage] initialSync started');
    setSyncStatus('syncing');
    try {
      const remoteUserOps = await this.getUserOps();
      if (remoteUserOps && Object.keys(remoteUserOps).length > 0) {
          await Worksheet.mergeRemote(Worksheet.TIERS.OPERATORS, null, remoteUserOps);
      }

      const remoteLayout = await this.getLayout();
      if (remoteLayout) {
        await Worksheet.mergeRemote(Worksheet.TIERS.WINDOWS, null, remoteLayout);
      }

      setSyncStatus('idle');
      console.log('[RemoteStorage] initialSync finished');
    } catch (e) {
      console.error('[RemoteStorage] Initial sync failed:', e);
      setSyncStatus('error');
    }
  },

  // --- GETTERS ---
  getUserOps: async () => {
      if (!rs) return {};
      const data = await rs.jotcad.getUserOps();
      if (data && typeof data === 'string') {
          try { return JSON.parse(data); } catch(e) { return {}; }
      }
      return data || {};
  },
  getLayout: async () => {
      if (!rs) return { windows: [], desktop: [] };
      const data = await rs.jotcad.getLayout();
      if (data && typeof data === 'string') {
          try { return JSON.parse(data); } catch(e) { return { windows: [], desktop: [] }; }
      }
      return data || { windows: [], desktop: [] };
  },

  saveUserOps: (data) => rs?.jotcad.saveUserOps(data),

  async pushLayout(layout) {
    if (!rs || !rs.connected) return;
    await rs.jotcad.saveLayout(layout);
    syncActions.updateShadowLayout(layout);
  },

  async pushOperator(path, script, schema) {
    if (!rs || !rs.connected || !path) return;
    const vfsKey = path.replace('user/', '');
    const currentOps = await this.getUserOps() || {};
    currentOps[vfsKey] = { script, schema };
    await rs.jotcad.saveUserOps(currentOps);
    syncActions.updateShadowOp(path, { script, schema });
  },

  async removeOperator(path) {
    if (!rs || !rs.connected || !path) return;
    const vfsKey = path.replace('user/', '');
    const currentOps = await this.getUserOps() || {};
    delete currentOps[vfsKey];
    await rs.jotcad.saveUserOps(currentOps);
    syncActions.removeShadowOp(path);
  }
};
