import { normalizeSelector, isMatch } from './vfs_core.js';
import crypto from 'crypto';
import { PassThrough } from 'stream';

/**
 * Attaches the Full Symmetric VFS REST protocol to an existing HTTP server.
 */
export function registerVFSRoutes(vfs, server, prefix = '/vfs') {
  const activePeers = new Map(); // PeerID -> { res }
  const pendingCommands = new Map(); // CommandID -> { res, timeout, selector }
  const activeSelectors = []; // Array of { selector, commandId }
  const unfulfilledDemands = []; // Array of { selector }

  const routeDemand = async (path, parameters, commandId = null) => {
    const selector = normalizeSelector(path, parameters);
    
    // Deduplication: If this exact selector is already being routed, don't double-send
    if (!commandId && activeSelectors.some(s => isMatch(s.selector, selector))) {
        return true;
    }

    console.log(`[VFS Server] Routing demand for: ${path} (cmd: ${commandId || 'new'})`);
    let routed = false;
    
    // Find a peer that is LISTENING for this PATH specifically
    for (const info of vfs.states.values()) {
      if (info.path === path && info.state === 'LISTENING' && info.sources) {
        for (const rawSource of info.sources) {
            const peerId = rawSource.startsWith('peer:') ? rawSource.slice(5) : rawSource;
            const targetRes = activePeers.get(peerId);
            
            if (targetRes) {
              const cid = commandId || crypto.randomUUID();
              console.log(`[VFS Server] Delivering COMMAND:READ to peer ${peerId} (id: ${cid})`);
              
              if (!commandId) {
                  activeSelectors.push({ selector, commandId: cid });
                  // Create a placeholder command so we can track the reply even if no one is /reading yet
                  pendingCommands.set(cid, { res: null, selector, timeout: setTimeout(() => {
                      pendingCommands.delete(cid);
                      const idx = activeSelectors.findIndex(s => s.commandId === cid);
                      if (idx !== -1) activeSelectors.splice(idx, 1);
                  }, 60000)});
              }

              targetRes.write(`data: ${JSON.stringify({
                type: 'COMMAND',
                id: cid,
                op: 'READ',
                path,
                parameters
              })}\n\n`);
              routed = true;
              const idx = unfulfilledDemands.findIndex(d => isMatch(d.selector, selector));
              if (idx !== -1) unfulfilledDemands.splice(idx, 1);
            }
        }
        if (routed) break; 
      }
    }

    if (!routed && !commandId) {
        if (!unfulfilledDemands.some(d => isMatch(d.selector, selector))) {
            unfulfilledDemands.push({ selector });
        }
    }
    return routed;
  };

  const reprocessDemands = async () => {
    if (unfulfilledDemands.length === 0) return;
    console.log(`[VFS Server] Reprocessing ${unfulfilledDemands.length} unfulfilled demands...`);
    const demands = [...unfulfilledDemands];
    for (const d of demands) {
        await routeDemand(d.selector.path, d.selector.parameters);
    }
  };

  vfs.events.on('state', async (event) => {
    try {
        if (event.state === 'PENDING' && event.source !== 'node') {
            await routeDemand(event.path, event.parameters);
        } else if (event.state === 'LISTENING') {
            await reprocessDemands();
        }
    } catch (err) { console.error('[VFS Server] State listener error:', err); }
  });

  server.on('request', async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!url.pathname.startsWith(prefix)) return;

    const vfsPath = url.pathname.slice(prefix.length);
    const peerId = req.headers['x-vfs-peer-id'] || url.searchParams.get('peerId') || 'default';

    const getBody = async () => {
      let body = '';
      for await (const chunk of req) body += chunk;
      try { return JSON.parse(body || '{}'); } catch (e) { return {}; }
    };

    try {
      if (req.method === 'GET' && vfsPath === '/watch') {
        res.writeHead(200, { 'Content-Type': 'text/event-stream', 'Cache-Control': 'no-cache', 'Connection': 'keep-alive' });
        const onState = (event) => {
          if (event.source === peerId || event.source === `peer:${peerId}`) return;
          const { cid, ...cleanEvent } = event;
          res.write(`data: ${JSON.stringify(cleanEvent)}\n\n`);
        };
        vfs.events.on('state', onState);
        activePeers.set(peerId, res);
        await reprocessDemands();
        req.on('close', () => { vfs.events.off('state', onState); activePeers.delete(peerId); });
        return;
      }

      if (req.method === 'POST' && vfsPath.startsWith('/reply/')) {
        const commandId = vfsPath.split('/')[2];
        const cmd = pendingCommands.get(commandId);
        
        if (!cmd) {
          console.warn(`[VFS Server] Received reply for unknown/expired command: ${commandId}`);
          res.writeHead(410);
          return res.end('Command expired or invalid');
        }

        console.log(`[VFS Server] Fulfilling read for ${cmd.selector.path} (cmd: ${commandId})`);
        clearTimeout(cmd.timeout);
        pendingCommands.delete(commandId);
        
        const idx = activeSelectors.findIndex(s => s.commandId === commandId);
        if (idx !== -1) activeSelectors.splice(idx, 1);

        const p1 = new PassThrough();
        const p2 = new PassThrough();
        req.pipe(p1);
        req.pipe(p2);

        vfs.write(cmd.selector.path, cmd.selector.parameters, p1).catch(err => {
            console.error(`[VFS Server] Failed to save mailbox reply for ${cmd.selector.path}:`, err);
        });

        if (cmd.res) {
            cmd.res.writeHead(200, { 'Content-Type': req.headers['content-type'] || 'application/octet-stream' });
            p2.pipe(cmd.res);
        } else {
            p2.on('data', () => {});
        }
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/state') {
        const event = await getBody();
        await vfs.receive({ source: peerId, ...event });
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/read') {
        const { path, parameters } = await getBody();
        const selector = normalizeSelector(path, parameters);
        const state = await vfs.status(selector.path, selector.parameters);
        
        if (state === 'AVAILABLE' || state === 'SCHEMA') {
            const stream = await vfs.read(selector.path, selector.parameters);
            res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
            if (stream.pipe) return stream.pipe(res);
            for await (const chunk of stream) res.write(chunk);
            return res.end();
        }

        let commandId;
        const existing = activeSelectors.find(s => isMatch(s.selector, selector));
        if (existing) {
            commandId = existing.commandId;
            const cmd = pendingCommands.get(commandId);
            if (cmd && !cmd.res) {
                cmd.res = res; 
                return;
            }
        }

        commandId = crypto.randomUUID();
        pendingCommands.set(commandId, { 
            res, 
            selector,
            timeout: setTimeout(() => {
                pendingCommands.delete(commandId);
                const idx = activeSelectors.findIndex(s => s.commandId === commandId);
                if (idx !== -1) activeSelectors.splice(idx, 1);
                if (!res.writableEnded) { res.writeHead(504); res.end('VFS Read Timeout'); }
            }, 60000)
        });
        activeSelectors.push({ selector, commandId });

        await routeDemand(path, parameters, commandId);
        return;
      }

      const body = (req.method === 'POST') ? await getBody() : null;
      if (vfsPath === '/cid') return res.end(JSON.stringify({ cid: await vfs.getCID(body) }));
      if (vfsPath === '/status') return res.end(JSON.stringify({ state: await vfs.status(body.path, body.parameters) }));
      if (vfsPath === '/tickle') return res.end(JSON.stringify({ state: await vfs.tickle(body.path, body.parameters) }));
      if (vfsPath === '/declare') { await vfs.declare(body.path, body.schema); return res.end(JSON.stringify({ success: true })); }
      if (vfsPath === '/link') { await vfs.link(body.source, body.target); return res.end(JSON.stringify({ success: true })); }
      if (vfsPath === '/lease') return res.end(JSON.stringify({ success: await vfs.lease(body.path, body.parameters, body.duration) }));

      if (req.method === 'PUT' && vfsPath === '/write') {
        const info = JSON.parse(req.headers['x-vfs-info'] || '{}');
        await vfs.write(info.path, info.parameters, req);
        res.writeHead(201);
        return res.end();
      }

      if (req.method === 'GET' && vfsPath === '/states') {
        const states = await Promise.all(Array.from(vfs.states.values()).map(async (info) => {
          let data = null;
          if (info.state === 'SCHEMA') {
            const cid = await vfs.getCID(info);
            const stream = await vfs.storage.get(cid);
            if (stream) {
              const chunks = [];
              for await (const chunk of stream) chunks.push(chunk);
              data = Buffer.concat(chunks).toString('utf-8');
            }
          }
          return { path: info.path, parameters: info.parameters, state: info.state, sources: info.sources, data };
        }));
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(states));
      }

    } catch (err) {
      console.error('[VFS REST Error]', err);
      if (!res.writableEnded) { res.writeHead(500); res.end(err.message); }
    }
  });
}
