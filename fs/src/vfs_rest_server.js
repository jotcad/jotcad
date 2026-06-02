import { pipeline } from 'stream/promises';
import { Readable } from 'node:stream';
import { normalizeSelector, Selector, encodeSafe, decodeSafe, getSelectorKey, decodeInfo } from './vfs_core.js';
import { log, info } from './log.js';
import { WebSocketServer } from 'ws';

export function registerVFSRoutes(vfs, server, prefix = '', meshLink = null) {
  const getBody = async (req) => {
    const chunks = [];
    for await (const chunk of req) chunks.push(chunk);
    const data = Buffer.concat(chunks).toString();
    return data ? JSON.parse(data) : {};
  };

  const getBinaryBody = async (req) => {
    const chunks = [];
    for await (const chunk of req) chunks.push(chunk);
    return Buffer.concat(chunks);
  };

  const parseHeaders = (req) => {
    const h = req.headers;
    const res = {
        op: h['x-vfs-op'],
        encoding: h['x-vfs-encoding'],
        expiresAt: h['x-vfs-expires'] ? parseInt(h['x-vfs-expires']) : null,
        replyTo: h['x-vfs-reply-to'],
        stack: h['x-vfs-stack'] ? h['x-vfs-stack'].split(',').map(s => s.trim()) : []
    };
    if (h['x-vfs-selector']) {
        try {
            res.selector = Selector.fromObject(JSON.parse(h['x-vfs-selector']));
        } catch (e) {
            try { res.selector = decodeSafe(h['x-vfs-selector']); } catch (e2) {}
        }
    }
    return res;
  };

  const encodeInfo = (info) => info ? JSON.stringify(info) : '';

  const pump = async (stream, res) => {
    try {
      await pipeline(Readable.fromWeb(stream), res);
    } catch (err) {
      console.error(`[REST SERVER PUMP ERROR]`, err);
      if (!res.writableEnded) res.end();
    }
  };

  const handleRequest = async (req, res) => {
    log(`[MeshServer ${vfs.id}] INCOMING: ${req.method} ${req.url}`);
    // 1. Global CORS Headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type, x-vfs-id, x-vfs-peer-id, x-vfs-reply-to, x-vfs-local-url, x-vfs-info, x-vfs-op, x-vfs-selector, x-vfs-stack, x-vfs-expires, x-vfs-encoding');
    res.setHeader('Access-Control-Expose-Headers', 'x-vfs-info, x-vfs-id, x-vfs-peer-id, x-vfs-op, x-vfs-selector, x-vfs-stack, x-vfs-expires, x-vfs-encoding');

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

      const vfsHeaders = parseHeaders(req);

      if (req.method === 'POST' && vfsPath === '/read_selector') {
        const { selector, stack, expiresAt } = vfsHeaders;
        const body = await getBody(req);
        const rawSel = selector || body.selector;
        const s = Selector.fromObject(rawSel);
        if (!s) {
            res.writeHead(400);
            return res.end('Missing or invalid selector');
        }
        log(`[MeshServer ${vfs.id}] POST /read_selector: Selector(${s.path})`);
        const result = await vfs.readSelector(s, { stack, expiresAt });

        if (result) {
          const { stream, metadata } = result;
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id,
            'x-vfs-info': encodeInfo(metadata),
            'x-vfs-encoding': metadata.encoding || 'json'
          });
          return await pump(stream, res);
        }
        res.writeHead(404, { 'x-vfs-info': encodeInfo({ error: 'Not Found' }) });
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/read_cid') {
        const { stack, expiresAt } = vfsHeaders;
        const body = await getBody(req);
        const cid = body.cid;
        const resolutionStack = body.resolutionStack || [];
        log(`[MeshServer ${vfs.id}] POST /read_cid: CID(${cid})`);
        const result = await vfs.readCID(cid, { stack, resolutionStack, expiresAt });

        if (result) {
          const { stream, metadata } = result;
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id,
            'x-vfs-info': encodeInfo(metadata),
            'x-vfs-encoding': metadata.encoding || 'json'
          });
          return await pump(stream, res);
        }
        res.writeHead(404, { 'x-vfs-info': encodeInfo({ error: 'Not Found' }) });
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/subscribe') {
        let selector, expiresAt, stack;

        if (vfsHeaders.selector) {
            selector = vfsHeaders.selector;
            expiresAt = vfsHeaders.expiresAt;
            stack = vfsHeaders.stack;
        } else {
            const body = await getBody(req);
            selector = Selector.fromObject(body.selector);
            expiresAt = body.expiresAt;
            stack = body.stack || [];
        }

        if (!selector) {
            res.writeHead(400);
            return res.end('Missing selector');
        }
        vfs.validateSelector(selector);
        const peerId = req.headers['x-vfs-id'] || stack[0] || 'unknown';
        meshLink?.addInterest(peerId, selector, expiresAt, stack);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/notify') {
        let selector, payload, stack;

        if (vfsHeaders.selector) {
            selector = vfsHeaders.selector;
            stack = vfsHeaders.stack;
            if (vfsHeaders.encoding === 'bytes') {
                payload = await getBinaryBody(req);
            } else {
                payload = await getBody(req);
            }
        } else {
            const body = await getBody(req);
            selector = body.selector ? Selector.fromObject(body.selector) : null;
            payload = body.payload;
            stack = body.stack || [];
        }

        info(`[MeshServer ${vfs.id}] POST /notify ${selector?.path || 'null'} from stack ${stack}`);
        if (selector) vfs.validateSelector(selector); 
        meshLink?.notify(selector, payload, stack);
        res.writeHead(200);
        return res.end();
      }

      if (req.method === 'POST' && vfsPath === '/spy') {
        let selector, expiresAt, stack;

        if (vfsHeaders.selector) {
            selector = vfsHeaders.selector;
            expiresAt = vfsHeaders.expiresAt;
            stack = vfsHeaders.stack;
        } else {
            const body = await getBody(req);
            selector = Selector.fromObject(body.selector);
            expiresAt = body.expiresAt;
            stack = body.stack || [];
        }

        if (!selector) {
            res.writeHead(400);
            return res.end('Missing selector');
        }
        vfs.validateSelector(selector);

        const stream = await vfs.spy(selector, { stack, expiresAt });
        if (stream) {
          res.writeHead(200, {
            'Content-Type': 'application/octet-stream',
            'x-vfs-id': vfs.id
          });
          await pump(stream, res);
        } else {
          res.writeHead(404);
          res.end();
        }
        return;
      }

      if (req.method === 'POST' && vfsPath === '/register') {
        let peerId = url.searchParams.get('id') || url.searchParams.get('peerId');
        let peerUrl = url.searchParams.get('url');

        let body = null;
        try {
          body = await getBody(req);
        } catch (e) {}

        if (body) {
          if (!peerId) peerId = body.id || body.peerId;
          if (!peerUrl) peerUrl = body.url;
        }

        log(`[MeshServer ${vfs.id}] POST /register: peerId=${peerId}, url=${peerUrl}`);
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing peer id');
        }

        if (meshLink) {
          const result = await meshLink.registerPeer(peerId, peerUrl);
          res.writeHead(200, { 'Content-Type': 'application/json' });
          return res.end(JSON.stringify(result));
        }
        res.writeHead(501);
        return res.end('MeshLink not initialized');
      }

      if (req.method === 'POST' && vfsPath === '/listen') {
        const peerId = req.headers['x-vfs-peer-id'];
        const replyTo = req.headers['x-vfs-reply-to'];
        const info = req.headers['x-vfs-info'];
        log(`[MeshServer ${vfs.id}] POST /listen from ${peerId} (replyTo: ${replyTo || 'none'})`);
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing x-vfs-peer-id');
        }
        if (meshLink) {
          // Protocol Integrity: Convert Node IncomingMessage to Web ReadableStream
          meshLink.registerReversePeer(peerId, res, replyTo, Readable.toWeb(req), info);
          return;
        }
        res.writeHead(501);
        return res.end('MeshLink not initialized');
      }

      if (req.method === 'GET' && vfsPath === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        return res.end(JSON.stringify({ status: 'OK', id: vfs.id }));
      }

      // Catch-all fallback for unhandled routes to prevent request hangs
      res.writeHead(404);
      return res.end('Not Found');
    } catch (err) {
      const vfsCtx = { vfsId: vfs.id, action: req.method + ' ' + req.url };
      const ctxLine = JSON.stringify(vfsCtx);
      
      let msg = err.message;
      if (!msg.trim().startsWith('{')) {
          msg = JSON.stringify({ error: msg });
      }
      
      const fullError = ctxLine + '\n' + msg;
      console.error(`[VFS ${vfs.id}] REST Error:\n${fullError}`);
      
      if (!res.writableEnded) {
        res.writeHead(500, { 'Content-Type': 'application/x-jsonlines' });
        res.end(fullError);
      }
    }
  };

  const wss = new WebSocketServer({ noServer: true });

  wss.on('connection', (ws) => {
    const onMessage = async (data) => {
      try {
        const frame = JSON.parse(data.toString());
        if (frame.type === 'IDENTIFY' && frame.peerId) {
          const peerId = frame.peerId;
          ws.off('message', onMessage);
          ws.send(JSON.stringify({ type: 'ACK', peerId: vfs.id }));

          const { WSNodeReverseConnection } = await import('./vfs_ws_transport_node.js');
          const activeConn = new WSNodeReverseConnection(peerId, ws);
          activeConn.vfs = vfs;
          activeConn.mesh = meshLink;
          meshLink?.upgradePeerToWS(peerId, activeConn);
          log(`[MeshServer ${vfs.id}] Upgraded peer ${peerId} over WebSocket Tunnel.`);
        } else {
          ws.close(4000, 'Expected IDENTIFY');
        }
      } catch (e) {
        log(`[MeshServer ${vfs.id}] WS Handshake error: ${e.message}`);
      }
    };
    ws.on('message', onMessage);
  });

  const handleUpgrade = (request, socket, head) => {
    const pathname = new URL(request.url, `http://${request.headers.host || 'localhost'}`).pathname;
    if (pathname === (prefix ? prefix + '/vfs-ws' : '/vfs-ws')) {
      wss.handleUpgrade(request, socket, head, (ws) => {
        wss.emit('connection', ws, request);
      });
    }
  };

  server.on('upgrade', handleUpgrade);
  server.on('request', handleRequest);

  return () => {
    server.off('request', handleRequest);
    server.off('upgrade', handleUpgrade);
    wss.close();
  };
}
