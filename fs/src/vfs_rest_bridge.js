import { normalizeSelector } from './vfs_core.js';

/**
 * Connects a local VFS instance to a remote REST-based VFS server.
 */
export class RESTBridge {
  constructor(vfs, baseUrl) {
    this.vfs = vfs;
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.eventSource = null;
  }

  async start() {
    // 0. Synchronize existing state from remote
    try {
      const resp = await fetch(`${this.baseUrl}/states`);
      if (resp.ok) {
        const states = await resp.json();
        for (const s of states) {
          // Add source: 'remote' to prevent immediate re-broadcast back to server
          this.vfs.receive({ ...s, source: 'remote' });
        }
      }
    } catch (err) {
      console.warn('[RESTBridge] Initial sync failed:', err.message);
    }

    // 1. Listen to Local Events and sync to Remote
    this.vfs.events.on('state', async (event) => {
      // Don't sync events that came from the remote server itself
      if (event.source === 'remote') return;
      
      // Also don't sync if the source is already 'node' (prevent loops)
      if (event.source === 'node') return;

      if (event.state === 'PENDING') {
        await fetch(`${this.baseUrl}/tickle`, {
          method: 'POST',
          body: JSON.stringify({ path: event.path, parameters: event.parameters })
        });
      } else if (event.state === 'AVAILABLE') {
        const stream = await this.vfs.storage.get(event.cid);
        if (stream) {
          await fetch(`${this.baseUrl}/res/${event.cid}`, {
            method: 'PUT',
            headers: {
              'x-vfs-info': JSON.stringify({ path: event.path, parameters: event.parameters })
            },
            body: stream,
            duplex: 'half'
          });
        }
      }
    });

    // 2. Listen to Remote Events (SSE) and sync to Local
    this.eventSource = new EventSource(`${this.baseUrl}/watch`);
    this.eventSource.onmessage = (e) => {
      const event = JSON.parse(e.data);
      this.vfs.receive(event);
    };

    // 3. Intercept storage.get to fetch from remote if missing
    const originalGet = this.vfs.storage.get.bind(this.vfs.storage);
    this.vfs.storage.get = async (cid) => {
      const local = await originalGet(cid);
      if (local) return local;

      // Try fetching from remote
      const resp = await fetch(`${this.baseUrl}/res/${cid}`);
      if (resp.ok) {
        // We don't necessarily want to cache it locally here, 
        // vfs.read() will call this and we return the remote stream.
        return resp.body;
      }
      return null;
    };
  }

  stop() {
    if (this.eventSource) {
      this.eventSource.close();
      this.eventSource = null;
    }
  }
}
