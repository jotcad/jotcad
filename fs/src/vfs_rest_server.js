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

  const decodeRecordBody = async (req) => {
    const chunks = [];
    for await (const chunk of req) chunks.push(chunk);
    const bodyBytes = Buffer.concat(chunks);
    if (bodyBytes.length < 4) throw new Error("Body too short for length prefix");
    const headerLen = bodyBytes.readUInt32BE(0);
    if (bodyBytes.length < 4 + headerLen) throw new Error("Body missing JSON header bytes");
    const headerStr = bodyBytes.subarray(4, 4 + headerLen).toString('utf8');
    const header = JSON.parse(headerStr);
    const payload = bodyBytes.subarray(4 + headerLen);
    return { header, payload };
  };

  const encodeRecordStream = (respHeader, payloadStream) => {
    const headerStr = JSON.stringify(respHeader);
    const headerBytes = new TextEncoder().encode(headerStr);
    const lenBuf = new Uint8Array(4);
    new DataView(lenBuf.buffer).setUint32(0, headerBytes.length, false);
    
    let reader = null;
    if (payloadStream) {
      reader = payloadStream.getReader();
    }
    
    return new ReadableStream({
      async start(controller) {
        controller.enqueue(lenBuf);
        controller.enqueue(headerBytes);
      },
      async pull(controller) {
        if (!reader) {
          controller.close();
          return;
        }
        const { done, value } = await reader.read();
        if (done) {
          controller.close();
        } else {
          controller.enqueue(value);
        }
      },
      cancel() {
        if (reader) reader.releaseLock();
      }
    });
  };

  const encodeRecord = (respHeader, payloadBytes = null) => {
    const headerStr = JSON.stringify(respHeader);
    const headerBytes = new TextEncoder().encode(headerStr);
    const payload = payloadBytes || new Uint8Array(0);
    const record = new Uint8Array(4 + headerBytes.length + payload.length);
    const view = new DataView(record.buffer);
    view.setUint32(0, headerBytes.length, false);
    record.set(headerBytes, 4);
    record.set(payload, 4 + headerBytes.length);
    return record;
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
      let op = vfsHeaders.op;
      let selector = vfsHeaders.selector;
      let stack = vfsHeaders.stack;
      let expiresAt = vfsHeaders.expiresAt;
      let cid = null;
      let resolutionStack = [];
      let payload = null;
      let isLegacy = !!op;

      let recordHeader = null;
      if (!isLegacy && req.method === 'POST') {
        try {
          const record = await decodeRecordBody(req);
          recordHeader = record.header;
          op = recordHeader.op;
          selector = recordHeader.selector ? Selector.fromObject(recordHeader.selector) : null;
          stack = recordHeader.stack || [];
          expiresAt = recordHeader.expiresAt || null;
          cid = recordHeader.cid || null;
          resolutionStack = recordHeader.resolutionStack || [];
          payload = record.payload;
        } catch (e) {
          isLegacy = true;
        }
      }

      if (req.method === 'POST' && vfsPath === '/read_selector') {
        const s = isLegacy ? (selector || (await getBody(req)).selector) : selector;
        const finalSelector = Selector.fromObject(s);
        if (!finalSelector) {
            const respHeader = { info: { error: 'Missing or invalid selector' }, encoding: 'json' };
            res.writeHead(400, { 'Content-Type': 'application/octet-stream' });
            return res.end(encodeRecord(respHeader));
        }
        log(`[MeshServer ${vfs.id}] POST /read_selector: Selector(${finalSelector.path})`);
        const result = await vfs.readSelector(finalSelector, { stack, expiresAt });

        if (result) {
          const { stream, metadata } = result;
          const respHeader = {
            info: metadata,
            encoding: metadata.encoding || 'json'
          };
          res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
          return await pump(encodeRecordStream(respHeader, stream), res);
        }
        const respHeader = { info: { error: 'Not Found' }, encoding: 'json' };
        res.writeHead(404, { 'Content-Type': 'application/octet-stream' });
        return res.end(encodeRecord(respHeader));
      }

      if (req.method === 'POST' && vfsPath === '/read_cid') {
        const finalCid = isLegacy ? (await getBody(req)).cid : cid;
        const finalResStack = isLegacy ? ((await getBody(req)).resolutionStack || []) : resolutionStack;
        log(`[MeshServer ${vfs.id}] POST /read_cid: CID(${finalCid})`);
        const result = await vfs.readCID(finalCid, { stack, resolutionStack: finalResStack, expiresAt });

        if (result) {
          const { stream, metadata } = result;
          const respHeader = {
            info: metadata,
            encoding: metadata.encoding || 'json'
          };
          res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
          return await pump(encodeRecordStream(respHeader, stream), res);
        }
        const respHeader = { info: { error: 'Not Found' }, encoding: 'json' };
        res.writeHead(404, { 'Content-Type': 'application/octet-stream' });
        return res.end(encodeRecord(respHeader));
      }

      if (req.method === 'POST' && vfsPath === '/subscribe') {
        let finalSelector = selector;
        let finalExpiresAt = expiresAt;
        let finalStack = stack;

        if (isLegacy) {
          if (!vfsHeaders.selector) {
            const body = await getBody(req);
            finalSelector = Selector.fromObject(body.selector);
            finalExpiresAt = body.expiresAt;
            finalStack = body.stack || [];
          }
        }

        if (!finalSelector) {
            const respHeader = { info: { error: 'Missing selector' }, encoding: 'json' };
            res.writeHead(400, { 'Content-Type': 'application/octet-stream' });
            return res.end(encodeRecord(respHeader));
        }
        vfs.validateSelector(finalSelector);
        const peerId = req.headers['x-vfs-id'] || finalStack[0] || 'unknown';
        meshLink?.addInterest(peerId, finalSelector, finalExpiresAt, finalStack);
        
        const respHeader = { info: { state: 'AVAILABLE' }, encoding: 'json' };
        res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
        return res.end(encodeRecord(respHeader));
      }

      if (req.method === 'POST' && vfsPath === '/notify') {
        let finalSelector = selector;
        let finalPayload = payload;
        let finalStack = stack;

        if (isLegacy) {
          if (vfsHeaders.selector) {
            finalPayload = vfsHeaders.encoding === 'bytes' ? await getBinaryBody(req) : await getBody(req);
          } else {
            const body = await getBody(req);
            finalSelector = body.selector ? Selector.fromObject(body.selector) : null;
            finalPayload = body.payload;
            finalStack = body.stack || [];
          }
        }

        info(`[MeshServer ${vfs.id}] POST /notify ${finalSelector?.path || 'null'} from stack ${finalStack}`);
        if (finalSelector) vfs.validateSelector(finalSelector); 
        meshLink?.notify(finalSelector, finalPayload, finalStack);
        
        const respHeader = { info: { state: 'AVAILABLE' }, encoding: 'json' };
        res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
        return res.end(encodeRecord(respHeader));
      }

      if (req.method === 'POST' && vfsPath === '/spy') {
        let finalSelector = selector;
        let finalExpiresAt = expiresAt;
        let finalStack = stack;

        if (isLegacy) {
          if (vfsHeaders.selector) {
            finalSelector = vfsHeaders.selector;
            finalExpiresAt = vfsHeaders.expiresAt;
            finalStack = vfsHeaders.stack;
          } else {
            const body = await getBody(req);
            finalSelector = Selector.fromObject(body.selector);
            finalExpiresAt = body.expiresAt;
            finalStack = body.stack || [];
          }
        }

        if (!finalSelector) {
            const respHeader = { info: { error: 'Missing selector' }, encoding: 'json' };
            res.writeHead(400, { 'Content-Type': 'application/octet-stream' });
            return res.end(encodeRecord(respHeader));
        }
        vfs.validateSelector(finalSelector);

        const stream = await vfs.spy(finalSelector, { stack: finalStack, expiresAt: finalExpiresAt });
        if (stream) {
          const respHeader = {
            info: { state: 'AVAILABLE' },
            encoding: 'bytes'
          };
          res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
          await pump(encodeRecordStream(respHeader, stream), res);
        } else {
          const respHeader = { info: { error: 'Not Found' }, encoding: 'json' };
          res.writeHead(404, { 'Content-Type': 'application/octet-stream' });
          res.end(encodeRecord(respHeader));
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
        let peerId = req.headers['x-vfs-peer-id'];
        let replyTo = req.headers['x-vfs-reply-to'];
        let info = req.headers['x-vfs-info'];
        let stream = Readable.toWeb(req);

        if (!peerId) {
          try {
            const record = await decodeRecordBody(req);
            peerId = record.header.peerId;
            replyTo = record.header.replyTo;
            info = record.header.info ? JSON.stringify(record.header.info) : null;
            if (record.payload && record.payload.length > 0) {
              stream = Readable.toWeb(Readable.from(record.payload));
            } else {
              stream = null;
            }
          } catch (e) {
            res.writeHead(400);
            return res.end('Missing x-vfs-peer-id or invalid record body');
          }
        }

        log(`[MeshServer ${vfs.id}] POST /listen from ${peerId} (replyTo: ${replyTo || 'none'})`);
        if (!peerId) {
          res.writeHead(400);
          return res.end('Missing x-vfs-peer-id');
        }
        if (meshLink) {
          meshLink.registerReversePeer(peerId, res, replyTo, stream, info);
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
