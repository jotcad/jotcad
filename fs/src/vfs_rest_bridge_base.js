/**
 * Base logic for bridging a local VFS instance to a remote REST server.
 * Environment-agnostic. Dependencies (EventSource, fetch) must be injected.
 */
export class RESTBridgeBase {
  constructor(vfs, baseUrl, { EventSource, fetch }) {
    this.vfs = vfs;
    this.baseUrl = baseUrl.replace(/\/$/, '');
    this.EventSource = EventSource;
    this.fetch = fetch;
    this.eventSource = null;
  }

  async start() {
    // 0. Initial Sync
    try {
      const resp = await this.fetch(`${this.baseUrl}/states`);
      if (resp.ok) {
        const states = await resp.json();
        for (const s of states) this.vfs.receive({ ...s, source: 'remote' });
      }
    } catch (err) { console.warn('[RESTBridge] Sync failed', err); }

    // 1. Local -> Remote (Outbound)
    this.vfs.events.on('state', async (event) => {
      if (event.source === 'remote' || event.source === 'node') return;
      if (!event.path) return; // Ignore lifecycle events like CLOSED

      // MESH SILENCE: We no longer push every local state change to a Hub.
      // Data and state remain local until explicitly requested via READ.
      /*
      await this.fetch(`${this.baseUrl}/state`, {
        method: 'POST',
        headers: { 
          'Content-Type': 'application/json',
          'x-vfs-peer-id': this.vfs.id
        },
        body: JSON.stringify(event)
      });
      */

      // MESH SILENCE: No more proactive PUSH of binary data.
      /*
      if (event.state === 'AVAILABLE') {
        const stream = await this.vfs.storage.get(event.cid);
        if (stream) {
          await this.fetch(`${this.baseUrl}/write`, {
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
      */
    });

    // 2. Remote -> Local (Inbound Commands via SSE)
    // Polyfill-like behavior for PeerID identification via Query Param
    this.eventSource = new this.EventSource(`${this.baseUrl}/watch?peerId=${this.vfs.id}`);

    this.eventSource.onmessage = async (e) => {
      const event = JSON.parse(e.data);
      
      // Handle Relayed Commands
      if (event.type === 'COMMAND') {
        if (event.op === 'READ') {
          const stream = await this.vfs.read(event.path, event.parameters);
          
          // TEE the stream so we can both reply to the Hub AND update local state
          const [s1, s2] = stream.tee();
          
          // 1. Reply to Hub
          const replyPromise = this.fetch(`${this.baseUrl}/reply/${event.id}`, {
            method: 'POST',
            body: s1,
            duplex: 'half'
          });

          // 2. Update local state (this unblocks local readData calls)
          await this.vfs.write(event.path, event.parameters, s2);
          await replyPromise;
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

      const resp = await this.fetch(`${this.baseUrl}/read`, {
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

  async declare(path, schema) {
    await this.fetch(`${this.baseUrl}/declare`, {
      method: 'POST',
      headers: { 
        'Content-Type': 'application/json',
        'x-vfs-peer-id': this.vfs.id
      },
      body: JSON.stringify({ path, schema })
    });
  }

  stop() {
    this.eventSource?.close();
  }
}
