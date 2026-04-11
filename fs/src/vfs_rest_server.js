import { normalizeSelector } from './vfs_core.js';

/**
 * registerVFSRoutes: Attaches the pure Peer-to-Peer Mesh protocol to an HTTP server.
 * 100% Synchronous, 100% Demand-Driven.
 */
export function registerVFSRoutes(vfs, server, prefix = '', meshLink = null) {
  const activeListeners = new Set();

  const handleRequest = async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!url.pathname.startsWith(prefix)) return;

    const vfsPath = url.pathname.slice(prefix.length);

    const getBody = () => new Promise((resolve) => {
      let body = '';
      req.on('data', chunk => body += chunk);
      req.on('end', () => resolve(JSON.parse(body || '{}')));
    });

    const pump = async (stream) => {
        const reader = stream.getReader();
        try {
            while (true) {
                const { done, value } = await reader.read();
                if (done) break;
                res.write(value);
            }
        } finally {
            reader.releaseLock();
            res.end();
        }
    };

    try {
      if (req.method === 'POST' && vfsPath === '/read') {
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
            res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'x-vfs-id': vfs.id });
            return await pump(stream);
        }

        res.writeHead(404);
        return res.end('Not Found');
      }

      if (req.method === 'GET' && vfsPath === '/watch') {
        res.writeHead(200, {
          'Content-Type': 'text/event-stream',
          'Cache-Control': 'no-cache',
          'Connection': 'keep-alive',
          'Access-Control-Allow-Origin': '*'
        });

        const onState = (event) => {
          res.write(`data: ${JSON.stringify(event)}\n\n`);
        };

        vfs.events.on('state', onState);
        activeListeners.add(onState);

        req.on('close', () => {
            vfs.events.off('state', onState);
            activeListeners.delete(onState);
        });
        return;
      }

      if (req.method === 'POST' && vfsPath === '/cid') {
        const body = await getBody();
        const cid = await vfs.getCID(body);
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ cid }));
      }

      if (req.method === 'GET' && vfsPath === '/peers') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify([...(meshLink?.peers?.keys() || [])]));
      }

      if (req.method === 'GET' && vfsPath === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'OK', id: vfs.id }));
      }

    } catch (err) {
      console.error(`[MeshServer ${vfs.id}] REST Error:`, err);
      if (!res.writableEnded) { res.writeHead(500); res.end(err.message); }
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
