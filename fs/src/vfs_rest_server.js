import { normalizeSelector } from './vfs_core.js';

/**
 * registerVFSRoutes: Attaches the pure Peer-to-Peer Mesh protocol to an HTTP server.
 * 100% Synchronous, 100% Demand-Driven.
 */
export function registerVFSRoutes(vfs, server, prefix = '', meshLink = null) {
  const activeListeners = new Set();

  const handleRequest = async (req, res) => {
    const timestamp = new Date().toISOString();
    // Add CORS headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader(
      'Access-Control-Allow-Headers',
      'Content-Type, x-vfs-peer-id, x-vfs-reply-to, x-vfs-local-url, x-vfs-id'
    );
    // Handle preflight
    if (req.method === 'OPTIONS') {
      res.writeHead(204);
      return res.end();
    }

    const url = new URL(req.url, `http://${req.headers.host}`);

    if (!url.pathname.startsWith(prefix)) {
      console.log(`[${timestamp}] [VFS Server] ?? ${req.method} ${url.pathname} (Unrouted)`);
      return;
    }

    const vfsPath = url.pathname.slice(prefix.length);
    console.log(`[${timestamp}] [VFS Server] -> ${req.method} ${vfsPath}`);

    const getBody = () =>
      new Promise((resolve) => {
        let body = '';
        req.on('data', (chunk) => (body += chunk));
        req.on('end', () => resolve(JSON.parse(body || '{}')));
      });

    const pump = async (stream) => {
      const reader = stream.getReader();
      try {
        while (true) {
          try {
            const { done, value } = await reader.read();
            if (done) break;
            res.write(value);
          } catch (e) {
            console.warn(
              '[VFS Server] Stream read error (peer disconnected):',
              e.message
            );
            break;
          }
        }
      } finally {
        reader.releaseLock();
        if (!res.writableEnded) res.end();
      }
    };

    try {
      if (req.method === 'POST' && vfsPath.trim() === '/read') {
        const body = await getBody();
        const s = normalizeSelector(body.path, body.parameters);
        const { stack = [], expiresAt } = body;

        // Auto-Peering (Symmetry)
        const peerUrl = req.headers['x-vfs-local-url'];
        if (peerUrl && meshLink) {
          meshLink.addPeer(peerUrl);
        }

        if (expiresAt && Date.now() > expiresAt) {
          res.writeHead(408);
          return res.end('Request Expired');
        }

        // Standard Mesh Read (Local -> MeshLink)
        const stream = await vfs.read(s.path, s.parameters, { stack });
        if (stream) {
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id,
          });
          return await pump(stream);
        }

        res.writeHead(404);
        return res.end('Not Found');
      }

      if (req.method === 'POST' && vfsPath.trim() === '/spy') {
        console.log('[VFS Server] Entering /spy handler');
        const body = await getBody();
        console.log('[VFS Server] /spy body:', body);
        const s = normalizeSelector(body.path, body.parameters);
        const { stack = [], expiresAt } = body;

        console.log(`[VFS Server] Spy request for: ${s.path}`, s.parameters);

        const peerUrl = req.headers['x-vfs-local-url'];
        if (peerUrl && meshLink) {
          meshLink.addPeer(peerUrl);
        }

        if (expiresAt && Date.now() > expiresAt) {
          console.log('[VFS Server] Spy request expired');
          res.writeHead(408);
          return res.end('Request Expired');
        }

        const stream = await vfs.spy(s.path, s.parameters, {
          stack,
          expiresAt,
        });
        if (stream) {
          console.log('[VFS Server] /spy stream found and returned');
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id,
          });
          return await pump(stream);
        }

        console.log(`[VFS Server] /spy: Not Found for ${s.path}`);
        res.writeHead(404);
        return res.end(`Not Found: ${s.path}`);
      }

      if (req.method === 'POST' && vfsPath === '/register') {
        const { id: peerId, url: peerUrl } = await getBody();
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing peer id');
        }

        const canReachDirect = await meshLink?.probeDirectReachability(peerUrl);

        // Add as a StaticPeer if we can reach it, but do it in the background
        if (canReachDirect && peerUrl && !meshLink?.peers.has(peerId)) {
          // Decouple to avoid recursive deadlock during mutual registration
          setTimeout(() => {
            meshLink?.addPeer(peerUrl);
          }, 0);
        }

        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(
          JSON.stringify({
            id: vfs.id,
            reachability: canReachDirect ? 'DIRECT' : 'REVERSE',
          })
        );
      }

      if (req.method === 'POST' && vfsPath === '/subscribe') {
        const body = await getBody();
        const { selector, expiresAt, stack } = body;
        const peerId = req.headers['x-vfs-id'] || 'unknown';
        meshLink?.addInterest(peerId, selector, expiresAt, stack || []);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/notify') {
        const body = await getBody();
        const { selector, payload, stack } = body;
        meshLink?.notify(selector, payload, stack || []);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/listen') {
        const peerId = req.headers['x-vfs-peer-id'];
        const replyTo = req.headers['x-vfs-reply-to'];
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing x-vfs-peer-id');
        }

        if (meshLink) {
          meshLink.registerReversePeer(peerId, res, replyTo, req);
          // Note: res is NOT ended here; it's held by MeshLink for the next command.
          return;
        }

        res.writeHead(501);
        return res.end('MeshLink not initialized');
      }

      if (req.method === 'GET' && vfsPath === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ status: 'OK', id: vfs.id }));
      }

      if (req.method === 'GET' && vfsPath === '/version') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(
          JSON.stringify({ version: vfs.version || 'unknown', id: vfs.id })
        );
      }
    } catch (err) {
      console.error(`[MeshServer ${vfs.id}] REST Error:`, err);
      if (!res.writableEnded) {
        res.writeHead(500);
        res.end(err.message);
      }
    }
  };

  server.on('request', handleRequest);

  return () => {
    server.off('request', handleRequest);
    for (const listener of activeListeners) {
      vfs.events.off('state', listener);
    }
    activeListeners.clear();
  };
}
