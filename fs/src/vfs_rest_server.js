import { normalizeSelector } from './vfs_core.js';
import crypto from 'crypto';

/**
 * Attaches the Full Symmetric VFS REST protocol to an existing HTTP server.
 * Supports Virtual Mailboxes for Peers behind firewalls.
 */
export function registerVFSRoutes(vfs, server, prefix = '/vfs') {
  const activePeers = new Map(); // PeerID -> { res, queue: [] }
  const pendingCommands = new Map(); // CommandID -> { resolve, reject }

  server.on('request', async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!url.pathname.startsWith(prefix)) return;

    const vfsPath = url.pathname.slice(prefix.length);
    const peerId = req.headers['x-vfs-peer-id'] || url.searchParams.get('peerId') || 'default';

    // Helper to parse JSON body
    const getBody = async () => {
      let body = '';
      for await (const chunk of req) body += chunk;
      return JSON.parse(body || '{}');
    };

    try {
      // 1. GET /watch - SSE stream with Command support
      if (req.method === 'GET' && vfsPath === '/watch') {
        console.log(`[VFS Server] Peer connected to watch: ${peerId}`);
        res.writeHead(200, {
          'Content-Type': 'text/event-stream',
          'Cache-Control': 'no-cache',
          'Connection': 'keep-alive'
        });

        const onState = (event) => {
          console.log(`[VFS Server] Pushing state to ${peerId}:`, event.path, event.state);
          res.write(`data: ${JSON.stringify(event)}\n\n`);
        };
        vfs.events.on('state', onState);

        // Register peer for command delivery
        activePeers.set(peerId, res);

        req.on('close', () => {
          vfs.events.off('state', onState);
          activePeers.delete(peerId);
        });
        return;
      }

      // 2. POST /peer/:targetId/read - Relay a read request to a private peer
      if (req.method === 'POST' && vfsPath.startsWith('/peer/')) {
        const parts = vfsPath.split('/');
        const targetPeerId = parts[2];
        const op = parts[3];

        if (op === 'read') {
          const { path, parameters } = await getBody();
          const targetRes = activePeers.get(targetPeerId);
          
          if (!targetRes) {
            res.writeHead(404);
            return res.end('Peer not connected');
          }

          const commandId = crypto.randomUUID();
          
          targetRes.write(`data: ${JSON.stringify({
            type: 'COMMAND',
            id: commandId,
            op: 'READ',
            path,
            parameters
          })}\n\n`);

          return new Promise((resolve, reject) => {
            pendingCommands.set(commandId, { 
                res, 
                timeout: setTimeout(() => {
                    pendingCommands.delete(commandId);
                    res.writeHead(504);
                    res.end('Peer timeout');
                    resolve();
                }, 30000)
            });
          });
        }
      }

      // 3. POST /reply/:commandId - Private peer fulfills a relayed command
      if (req.method === 'POST' && vfsPath.startsWith('/reply/')) {
        const commandId = vfsPath.split('/')[2];
        const cmd = pendingCommands.get(commandId);
        
        if (!cmd) {
          res.writeHead(410);
          return res.end('Command expired or invalid');
        }

        clearTimeout(cmd.timeout);
        pendingCommands.delete(commandId);

        cmd.res.writeHead(200, { 'Content-Type': req.headers['content-type'] });
        req.pipe(cmd.res);
        
        res.writeHead(200);
        return res.end();
      }

      // --- Standard Path-based Endpoints ---

      if (req.method === 'POST' && vfsPath === '/tickle') {
        const { path, parameters } = await getBody();
        const state = await vfs.tickle(path, parameters);
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ state }));
      }

      if (req.method === 'POST' && vfsPath === '/state') {
        const event = await getBody();
        await vfs.receive({ ...event, source: 'node' });
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/declare') {
        const { path, schema } = await getBody();
        await vfs.declare(path, schema);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/read') {
        const { path, parameters } = await getBody();
        const stream = await vfs.read(path, parameters);
        res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
        if (stream.pipe) return stream.pipe(res);
        for await (const chunk of stream) res.write(chunk);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/status') {
        const { path, parameters } = await getBody();
        const state = await vfs.status(path, parameters);
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ state }));
      }
if (req.method === 'POST' && vfsPath === '/lease') {
  const { path, parameters, duration } = await getBody();
  const success = await vfs.lease(path, parameters, duration);
  res.writeHead(200, { 'Content-Type': 'application/json' });
  return res.end(JSON.stringify({ success }));
}

// 5. POST /link - Create an alias
if (req.method === 'POST' && vfsPath === '/link') {
  const { source, target } = await getBody();
  await vfs.link(source, target);
  res.writeHead(200);
  return res.end();
}

// 6. PUT /write - Upload result

      if (req.method === 'PUT' && vfsPath === '/write') {
        const info = JSON.parse(req.headers['x-vfs-info'] || '{}');
        const deps = JSON.parse(req.headers['x-vfs-deps'] || '[]');
        await vfs.write(info.path, info.parameters, req, { dependencies: deps });
        res.writeHead(201);
        return res.end();
      }

      if (req.method === 'GET' && vfsPath === '/states') {
        const states = await Promise.all(Array.from(vfs.states.entries()).map(async ([cid, info]) => {
          let data = null;
          if (info.state === 'SCHEMA') {
            const stream = await vfs.storage.get(cid);
            if (stream) {
              const chunks = [];
              for await (const chunk of stream) chunks.push(chunk);
              data = Buffer.concat(chunks);
            }
          }
          return {
            cid, path: info.path, parameters: info.parameters, state: info.state, data
          };
        }));
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(states));
      }

    } catch (err) {
      console.error('[VFS REST Error]', err);
      res.writeHead(500);
      res.end(JSON.stringify({ error: err.message }));
    }
  });
}
