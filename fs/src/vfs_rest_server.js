import { normalizeSelector } from './vfs_core.js';

/**
 * Attaches VFS REST endpoints to an existing HTTP server.
 * @param {VFS} vfs The VFS instance to expose.
 * @param {http.Server} server The HTTP server.
 * @param {string} prefix URL prefix for the endpoints.
 */
export function registerVFSRoutes(vfs, server, prefix = '/vfs') {
  server.on('request', async (req, res) => {
    const url = new URL(req.url, `http://${req.headers.host}`);
    if (!url.pathname.startsWith(prefix)) return;

    const vfsPath = url.pathname.slice(prefix.length);

    try {
      // 1. GET /res/:cid - Download artifact
      if (req.method === 'GET' && vfsPath.startsWith('/res/')) {
        const cid = vfsPath.slice(5);
        const stream = await vfs.storage.get(cid);
        if (!stream) {
          res.writeHead(404);
          return res.end();
        }
        res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
        // Handle both Node and Browser stream piping
        if (stream.pipe) {
            stream.pipe(res);
        } else {
            for await (const chunk of stream) res.write(chunk);
            res.end();
        }
        return;
      }

      // 2. POST /tickle - Express demand
      if (req.method === 'POST' && vfsPath === '/tickle') {
        let body = '';
        for await (const chunk of req) body += chunk;
        const { path, parameters } = JSON.parse(body);
        const status = await vfs.tickle(path, parameters);
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ status }));
      }

      // 3. GET /watch - Server-Sent Events for state changes
      if (req.method === 'GET' && vfsPath === '/watch') {
        res.writeHead(200, {
          'Content-Type': 'text/event-stream',
          'Cache-Control': 'no-cache',
          'Connection': 'keep-alive'
        });

        const onState = (event) => {
          res.write(`data: ${JSON.stringify(event)}\n\n`);
        };

        vfs.events.on('state', onState);
        req.on('close', () => vfs.events.off('state', onState));
        return;
      }

      // 5. GET /states - Synchronize current state
      if (req.method === 'GET' && vfsPath === '/states') {
        const states = Array.from(vfs.states.entries()).map(([cid, info]) => ({
          cid,
          path: info.path,
          parameters: info.parameters,
          state: info.state
        }));
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify(states));
      }

      // 4. PUT /res/:cid - Upload result
      if (req.method === 'PUT' && vfsPath.startsWith('/res/')) {
        const cid = vfsPath.slice(5);
        // In a real system, we'd look up the path/params for this CID from the Request Plane first.
        // For now, let's assume we know what we're writing.
        // This endpoint requires metadata to be complete.
        // Let's simplify: client sends metadata in headers.
        const info = JSON.parse(req.headers['x-vfs-info'] || '{}');
        await vfs.write(info.path, info.parameters, req);
        res.writeHead(201);
        return res.end();
      }

    } catch (err) {
      console.error('[VFS REST Error]', err);
      res.writeHead(500);
      res.end(err.message);
    }
  });
}
