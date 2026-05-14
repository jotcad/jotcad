import { pipeline } from 'stream/promises';
import { Readable } from 'node:stream';
import { normalizeSelector, Selector, encodeSafe, getSelectorKey } from './vfs_core.js';

export function registerVFSRoutes(vfs, server, prefix = '', meshLink = null) {
  const getBody = async (req) => {
    const chunks = [];
    for await (const chunk of req) chunks.push(chunk);
    const data = Buffer.concat(chunks).toString();
    return data ? JSON.parse(data) : {};
  };

  const encodeInfo = (info) => info ? Buffer.from(JSON.stringify(info)).toString('base64') : '';
  const decodeInfo = (b64) => b64 ? JSON.parse(Buffer.from(b64, 'base64').toString()) : null;

  const pump = async (stream, res) => {
    try {
      await pipeline(Readable.fromWeb(stream), res);
    } catch (err) {
      if (!res.writableEnded) res.end();
    }
  };

  const handleRequest = async (req, res) => {
    // 1. Global CORS Headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-vfs-id, x-vfs-peer-id, x-vfs-reply-to, x-vfs-local-url');
    res.setHeader('Access-Control-Expose-Headers', 'x-vfs-info, x-vfs-id, x-vfs-peer-id');

    // 2. Handle Preflight
    if (req.method === 'OPTIONS') {
        res.writeHead(204);
        return res.end();
    }

    try {
      const url = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
      let vfsPath = url.pathname;
      if (prefix && vfsPath.startsWith(prefix)) {
        vfsPath = vfsPath.slice(prefix.length);
      }

      if (req.method === 'POST' && vfsPath === '/read') {
        const body = await getBody(req);
        const { selector: selObj, cid, stack = [], resolutionStack = [], expiresAt } = body;
        console.log(`[MeshServer ${vfs.id}] POST /read: ${selObj ? selObj.path : cid} (stack: ${stack.join('->')})`);
        if (!selObj && !cid) {
            res.writeHead(400);
            return res.end('Missing selector or cid');
        }
        const selector = selObj ? Selector.fromObject(selObj) : null;
        if (selector) vfs.validateSelector(selector); // CRITICAL: FAIL VISIBLY
        const target = selector || cid;
        const stream = await vfs.read(target, { stack, resolutionStack, expiresAt });
        if (stream) {
          const addrKey = typeof target === 'string' ? target : await getSelectorKey(target);
          const info = await vfs._getStorageInfo(addrKey);
          
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id,
            'x-vfs-info': encodeInfo(info),
          });
          return await pump(stream, res);
        }
        console.log(`[MeshServer ${vfs.id}] /read 404: ${target.path || target}`);
        res.writeHead(404);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/spy') {
        const body = await getBody(req);
        const { selector: selObj, stack = [], resolutionStack = [], expiresAt } = body;
        if (!selObj) {
            res.writeHead(400);
            return res.end('Missing selector');
        }
        const selector = Selector.fromObject(selObj);
        vfs.validateSelector(selector); // CRITICAL: FAIL VISIBLY
        const stream = await vfs.spy(selector, { stack, resolutionStack, expiresAt });
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
        const { selector: selObj, expiresAt, stack } = body;
        if (!selObj) {
            res.writeHead(400);
            return res.end('Missing selector');
        }
        const selector = Selector.fromObject(selObj);
        vfs.validateSelector(selector); // CRITICAL: FAIL VISIBLY
        const peerId = req.headers['x-vfs-id'] || 'unknown';
        meshLink?.addInterest(peerId, selector, expiresAt, stack || []);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/notify') {
        const body = await getBody(req);
        const { selector: selObj, payload, stack } = body;
        const selector = selObj ? Selector.fromObject(selObj) : null;
        console.log(`[MeshServer ${vfs.id}] POST /notify ${selector?.path || 'null'} from stack ${stack}`);
        if (selector) vfs.validateSelector(selector); // CRITICAL: FAIL VISIBLY
        meshLink?.notify(selector, payload, stack || []);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/listen') {
        const peerId = req.headers['x-vfs-peer-id'];
        const replyTo = req.headers['x-vfs-reply-to'];
        console.log(`[MeshServer ${vfs.id}] POST /listen from ${peerId} (replyTo: ${replyTo || 'none'})`);
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing x-vfs-peer-id');
        }
        if (meshLink) {
          // Protocol Integrity: Convert Node IncomingMessage to Web ReadableStream
          meshLink.registerReversePeer(peerId, res, replyTo, Readable.toWeb(req));
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
