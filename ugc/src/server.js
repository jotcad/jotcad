import http from 'node:http';
import https from 'node:https';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

export function startSessionServer(port, session, host = '0.0.0.0', sslOptions = null) {
  const publicDir = path.join(__dirname, 'public');

  const requestHandler = async (req, res) => {
    const url = new URL(req.url, `http://${host}:${port}`);
    const pathname = url.pathname;

    // CORS Headers
    res.setHeader('Access-Control-Allow-Origin', '*');
    res.setHeader('Access-Control-Allow-Methods', 'GET, OPTIONS');
    res.setHeader('Access-Control-Allow-Headers', 'Content-Type');

    if (req.method === 'OPTIONS') {
      res.writeHead(204);
      res.end();
      return;
    }

    try {
      // 1. GET /api/session -> Lists all snapshots in the current session
      if (pathname === '/api/session' && req.method === 'GET') {
        const snapshots = session.listSnapshots();
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ sessionId: session.sessionDir, snapshots }));
        return;
      }

      // 2. GET /api/session/snapshots/:snapshotDir/:filename -> Serves snapshot assets
      const snapshotFileMatch = pathname.match(/^\/api\/session\/snapshots\/([^/]+)\/([^/]+)$/);
      if (snapshotFileMatch && req.method === 'GET') {
        const snapshotDirName = decodeURIComponent(snapshotFileMatch[1]);
        const filename = decodeURIComponent(snapshotFileMatch[2]);

        const filePath = path.join(session.sessionDir, snapshotDirName, filename);

        // Security check: Make sure file path remains inside session directory
        if (!filePath.startsWith(session.sessionDir)) {
          res.writeHead(403);
          res.end('Access Denied');
          return;
        }

        if (fs.existsSync(filePath)) {
          const ext = path.extname(filePath).toLowerCase();
          let contentType = 'application/octet-stream';
          if (ext === '.json') contentType = 'application/json';
          if (ext === '.png') contentType = 'image/png';
          if (ext === '.stl') contentType = 'model/stl';
          if (ext === '.jot') contentType = 'text/plain';

          res.writeHead(200, { 'Content-Type': contentType });
          fs.createReadStream(filePath).pipe(res);
        } else {
          res.writeHead(404);
          res.end('File Not Found');
        }
        return;
      }

      // 3. Serve web/ library files statically for browser module imports
      if (pathname.startsWith('/web/')) {
        const rootDir = path.resolve(__dirname, '../..');
        const filePath = path.join(rootDir, pathname);

        if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
          const ext = path.extname(filePath).toLowerCase();
          let contentType = 'application/javascript';
          if (ext === '.json') contentType = 'application/json';

          res.writeHead(200, { 'Content-Type': contentType });
          fs.createReadStream(filePath).pipe(res);
          return;
        }
      }

      // 4. Static files serving (HTML, CSS, JS)
      let relativePath = pathname === '/' ? 'index.html' : pathname.slice(1);
      const filePath = path.join(publicDir, relativePath);

      if (fs.existsSync(filePath) && fs.statSync(filePath).isFile()) {
        const ext = path.extname(filePath).toLowerCase();
        let contentType = 'text/html';
        if (ext === '.css') contentType = 'text/css';
        if (ext === '.js') contentType = 'application/javascript';
        if (ext === '.png') contentType = 'image/png';

        res.writeHead(200, { 'Content-Type': contentType });
        fs.createReadStream(filePath).pipe(res);
      } else {
        res.writeHead(404);
        res.end('Not Found');
      }

    } catch (err) {
      console.error('[SessionServer] Server Error:', err);
      res.writeHead(500);
      res.end(`Internal Server Error: ${err.message}`);
    }
  };

  let server;
  let isHttps = false;

  // Auto-detect SSL keys in root if not explicitly provided
  const rootSslDir = path.resolve(__dirname, '../../.ssl');
  const defaultSslOptions = {
    keyPath: path.join(rootSslDir, 'localhost-key.pem'),
    certPath: path.join(rootSslDir, 'localhost-cert.pem')
  };
  const activeSsl = sslOptions || defaultSslOptions;

  if (fs.existsSync(activeSsl.keyPath) && fs.existsSync(activeSsl.certPath)) {
    isHttps = true;
    console.log(`[SessionServer] Loading SSL certificates:\n  - Key: ${activeSsl.keyPath}\n  - Cert: ${activeSsl.certPath}`);
    server = https.createServer({
      key: fs.readFileSync(activeSsl.keyPath),
      cert: fs.readFileSync(activeSsl.certPath)
    }, requestHandler);
  } else {
    server = http.createServer(requestHandler);
  }

  server.listen(port, host, () => {
    const protocol = isHttps ? 'https' : 'http';
    console.log(`[SessionServer] Listening at ${protocol}://${host}:${port}`);
  });

  return {
    server,
    isHttps,
    stop: () => {
      return new Promise((resolve) => {
        server.close(() => {
          resolve();
        });
      });
    }
  };
}
