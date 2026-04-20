import { pipeline } from 'stream/promises';
import { Readable } from 'node:stream';
import { normalizeSelector } from './vfs_core.js';

export function registerVFSRoutes(vfs, server, prefix = '', meshLink = null) {
  const getBody = async (req) => {
    const chunks = [];
    for await (const chunk of req) chunks.push(chunk);
    const data = Buffer.concat(chunks).toString();
    return data ? JSON.parse(data) : {};
  };

  const pump = async (stream, res) => {
    try {
      await pipeline(Readable.fromWeb(stream), res);
    } catch (err) {
      if (!res.writableEnded) res.end();
    }
  };

  const handleRequest = async (req, res) => {
    try {
      const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
      let vfsPath = url.pathname;
      if (prefix && vfsPath.startsWith(prefix)) {
        vfsPath = vfsPath.slice(prefix.length);
      }

      if (req.method === 'POST' && vfsPath === '/read') {
        const body = await getBody(req);
        const { path, parameters, stack = [], expiresAt } = body;
        const stream = await vfs.read({ path, parameters }, { stack, expiresAt });
        if (stream) {
          res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'x-vfs-id': vfs.id });
          return await pump(stream, res);
        }
        res.writeHead(404);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/spy') {
        const body = await getBody(req);
        const { path, parameters, stack = [], expiresAt } = body;
        const stream = await vfs.spy({ path, parameters }, { stack, expiresAt });
        if (stream) {
          res.writeHead(200, { 'Content-Type': 'application/octet-stream', 'x-vfs-id': vfs.id });
          return await pump(stream, res);
        }
        res.writeHead(404);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/register') {
        const { id: peerId, url: peerUrl } = await getBody(req);
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing peer id');
        }

        const canReachDirect = await meshLink?.probeDirectReachability(peerUrl);

        if (canReachDirect && peerUrl && !meshLink?.peers.has(peerId)) {
          setTimeout(() => { meshLink?.addPeer(peerUrl); }, 0);
        }

        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ id: vfs.id, reachability: canReachDirect ? 'DIRECT' : 'REVERSE' }));
      }

      if (req.method === 'POST' && vfsPath === '/subscribe') {
        const body = await getBody(req);
        const { selector, expiresAt, stack } = body;
        const peerId = req.headers['x-vfs-id'] || 'unknown';
        meshLink?.addInterest(peerId, selector, expiresAt, stack || []);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/notify') {
        const body = await getBody(req);
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
        return res.end(JSON.stringify({ version: vfs.version || 'unknown', id: vfs.id }));
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
  };
}
