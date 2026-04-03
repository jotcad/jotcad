/**
 * Bridges a local VFS instance to a remote REST server.
 * Handles targeted COMMAND events for symmetric peer behavior.
 */
export class RESTBridge {
  constructor(vfs, baseUrl) {
    this.vfs = vfs;
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.eventSource = null;
  }

  async start() {
    // 0. Initial Sync
    try {
      const resp = await fetch(`${this.baseUrl}/states`);
      if (resp.ok) {
        const states = await resp.json();
        for (const s of states) this.vfs.receive({ ...s, source: 'remote' });
      }
    } catch (err) { console.warn('[RESTBridge] Sync failed', err); }

    // 1. Local -> Remote (Outbound)
    this.vfs.events.on('state', async (event) => {
      if (event.source === 'remote' || event.source === 'node') return;

      if (event.state === 'PENDING') {
        await fetch(`${this.baseUrl}/tickle`, {
          method: 'POST',
          headers: { 
            'Content-Type': 'application/json',
            'x-vfs-peer-id': this.vfs.id
          },
          body: JSON.stringify({ path: event.path, parameters: event.parameters })
        });
      } else if (event.state === 'AVAILABLE') {
        const stream = await this.vfs.storage.get(event.cid);
        if (stream) {
          await fetch(`${this.baseUrl}/write`, {
            method: 'PUT',
            headers: { 
                'x-vfs-info': JSON.stringify({ path: event.path, parameters: event.parameters }),
                'x-vfs-peer-id': this.vfs.id
            },
            body: stream,
            duplex: 'half'
          });
        }
      }
    });

    // 2. Remote -> Local (Inbound Commands via SSE)
    this.eventSource = new EventSource(`${this.baseUrl}/watch`, {
        // Pass our PeerID so the Hub knows which mailbox we are watching
        headers: { 'x-vfs-peer-id': this.vfs.id } // Note: Standard EventSource doesn't support headers, we'd need a polyfill or use URL params
    });
    
    // Polyfill-like behavior for PeerID identification via Query Param
    if (this.eventSource.url) {
        this.eventSource.close();
        this.eventSource = new EventSource(`${this.baseUrl}/watch?peerId=${this.vfs.id}`);
    }

    this.eventSource.onmessage = async (e) => {
      const event = JSON.parse(e.data);
      
      // Handle Relayed Commands
      if (event.type === 'COMMAND') {
        if (event.op === 'READ') {
          console.log(`[RESTBridge] Fulfilling relayed READ: ${event.path}`);
          const stream = await this.vfs.read(event.path, event.parameters);
          
          // Reply back to the Hub
          await fetch(`${this.baseUrl}/reply/${event.id}`, {
            method: 'POST',
            body: stream,
            duplex: 'half'
          });
        }
        return;
      }

      this.vfs.receive({ ...event, source: 'node' });
    };

    // 3. Transparent Fetch
    const originalGet = this.vfs.storage.get.bind(this.vfs.storage);
    this.vfs.storage.get = async (cid) => {
      const local = await originalGet(cid);
      if (local) return local;

      const info = this.vfs.states.get(cid);
      if (!info) return null;

      const resp = await fetch(`${this.baseUrl}/read`, {
        method: 'POST',
        headers: { 
            'Content-Type': 'application/json',
            'x-vfs-peer-id': this.vfs.id
        },
        body: JSON.stringify({ path: info.path, parameters: info.parameters })
      });
      return resp.ok ? resp.body : null;
    };
  }

  stop() {
    this.eventSource?.close();
  }
}
