import { normalizeSelector } from './vfs_core.js';

/**
 * registerVFSRoutes: Attaches the Peer-to-Peer Mesh protocol to an HTTP server.
 */
export function registerVFSRoutes(vfs, server, prefix = '/vfs', meshLink = null) {
  server.on('request', async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!url.pathname.startsWith(prefix)) return;

    const vfsPath = url.pathname.slice(prefix.length);

    const getBody = async () => {
      let body = '';
      for await (const chunk of req) body += chunk;
      try { return JSON.parse(body || '{}'); } catch (e) { return {}; }
    };

    try {
      if (req.method === 'POST' && vfsPath === '/read') {
        const { selector: rawSelector, stack = [], expiresAt } = await getBody();
        const s = normalizeSelector(rawSelector);
        
        if (expiresAt && Date.now() > expiresAt) {
            res.writeHead(408);
            return res.end('Request Timeout');
        }

        if (meshLink?.localUrl && stack.includes(meshLink.localUrl)) {
            res.writeHead(404);
            return res.end('Loop Detected');
        }

        if (meshLink && stack.length > 0) {
            const senderUrl = stack[stack.length - 1];
            meshLink.addNeighbor(senderUrl);
        }

        const cid = await vfs.getCID(s);
        console.log(`[MeshServer ${vfs.id}] READ: ${s.path} Stack: ${stack.join(' -> ')}`);

        // Helper to pump Web Stream to Node Response
        const pump = async (stream) => {
            const reader = stream.getReader();
            let chunkCount = 0;
            try {
                while (true) {
                    const { done, value } = await reader.read();
                    if (done) {
                        console.log(`[MeshServer ${vfs.id}] PUMP DONE for ${s.path}. Total chunks: ${chunkCount}`);
                        break;
                    }
                    chunkCount++;
                    if (!res.write(value)) {
                        await new Promise(resolve => res.once('drain', resolve));
                    }
                }
            } catch (err) {
                console.error(`[MeshServer ${vfs.id}] PUMP ERROR for ${s.path}:`, err);
            } finally {
                reader.releaseLock();
                res.end();
            }
        };

        // 1. Try Local
        const localStream = await vfs.localRead(s.path, s.parameters);
        if (localStream) {
            console.log(`[MeshServer ${vfs.id}] HIT: ${s.path}`);
            res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
            return await pump(localStream);
        }

        // 2. Try Mesh
        if (meshLink) {
            const meshStream = await meshLink.read(s.path, s.parameters, stack);
            if (meshStream) {
                console.log(`[MeshServer ${vfs.id}] MESH FULFILL: ${s.path}`);
                res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
                return await pump(meshStream);
            }
        }

        res.writeHead(404);
        return res.end('Not Found');
      }

      if (req.method === 'GET' && vfsPath === '/peers') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(meshLink ? [...meshLink.neighbors] : []));
      }

      if (req.method === 'GET' && vfsPath === '/health') {
        res.writeHead(200);
        return res.end('OK');
      }

    } catch (err) {
      console.error(`[MeshServer ${vfs.id}] REST Error:`, err);
      if (!res.writableEnded) { res.writeHead(500); res.end(err.message); }
    }
  });
}
