import http from 'http';
import https from 'https';
import fs from 'fs';
import { spawn } from 'child_process';
import { Readable } from 'stream';
import { pipeline } from 'stream/promises';
import path from 'path';
import { VFS, DiskStorage, Selector, MeshLink, registerVFSRoutes } from './fs/src/index.js';

const id = process.env.VFS_ID || 'webcam-node';
const PORT = parseInt(process.env.PORT || '8080');
const DEVICE = process.env.WEBCAM_DEVICE || '/dev/video0';
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);

// 1. Initialize the VFS instance
const storageDir = path.resolve(`.vfs_storage_${id}`);
const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

// 2. Register the VFS provider for the webcam capture operation
vfs.registerProvider('jot/webcam/capture', async (vfsInstance, selector, context) => {
  const ffmpeg = spawn('ffmpeg', [
    '-f', 'v4l2',
    '-i', DEVICE,
    '-frames:v', '1',
    '-f', 'image2pipe',
    '-vcodec', 'mjpeg',
    '-'
  ]);

  const webStream = Readable.toWeb(ffmpeg.stdout);

  // Error handling for ffmpeg process
  ffmpeg.on('error', (err) => {
    console.error('[Webcam Provider] ffmpeg process error:', err);
  });

  return {
    stream: webStream,
    metadata: {
      encoding: 'bytes',
      filename: 'capture.jpg'
    }
  };
});

// Helper to prune old cached files from storageDir in the background
async function pruneStorage() {
  try {
    const files = await fs.promises.readdir(storageDir);
    const now = Date.now();
    for (const file of files) {
      if (file.endsWith('.data') || file.endsWith('.meta')) {
        const filePath = path.join(storageDir, file);
        const stats = await fs.promises.stat(filePath);
        if (now - stats.mtimeMs > 10000) { // Keep last 10 seconds of captures
          await fs.promises.unlink(filePath).catch(() => {});
        }
      }
    }
  } catch (e) {}
}

// 3. Detect and configure SSL certificates
const sslDir = path.resolve('./.ssl');
const keyPath = path.join(sslDir, 'localhost-key.pem');
const certPath = path.join(sslDir, 'localhost-cert.pem');

let server;
let protocol = 'http';

const requestHandler = async (req, res) => {
  const url = new URL(req.url, `${protocol}://${req.headers.host || 'localhost'}`);

  if (url.pathname === '/' || url.pathname === '/index.html') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(`
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>JotCAD VFS Webcam Server</title>
  <style>
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      background: #121214;
      color: #e1e1e6;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;
      min-height: 100vh;
      margin: 0;
      padding: 20px;
      box-sizing: border-box;
    }
    h1 {
      margin-bottom: 20px;
      font-size: 24px;
      font-weight: 600;
    }
    .video-container {
      background: #202024;
      border-radius: 8px;
      padding: 10px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.5);
      max-width: 100%;
    }
    img {
      display: block;
      max-width: 100%;
      height: auto;
      border-radius: 4px;
      background: #000;
    }
    .controls {
      margin-top: 20px;
      display: flex;
      gap: 10px;
    }
    button {
      background: #4f46e5;
      color: #fff;
      border: none;
      padding: 10px 20px;
      border-radius: 6px;
      font-size: 14px;
      cursor: pointer;
      font-weight: 500;
      transition: background 0.2s;
    }
    button:hover {
      background: #4338ca;
    }
  </style>
</head>
<body>
  <h1>VFS Webcam Feed</h1>
  <div class="video-container">
    <img id="feed" src="/image" alt="Webcam Live Feed" />
  </div>
  <div class="controls">
    <button onclick="refreshImage()">Refresh Frame</button>
    <button id="toggle-stream" onclick="toggleStream()">Start Auto-Refresh</button>
  </div>

  <script>
    let intervalId = null;
    const feed = document.getElementById('feed');
    const toggleBtn = document.getElementById('toggle-stream');

    function refreshImage() {
      feed.src = '/image?t=' + Date.now();
    }

    function toggleStream() {
      if (intervalId) {
        clearInterval(intervalId);
        intervalId = null;
        toggleBtn.innerText = 'Start Auto-Refresh';
        toggleBtn.style.background = '#4f46e5';
      } else {
        refreshImage();
        intervalId = setInterval(refreshImage, 1000);
        toggleBtn.innerText = 'Stop Auto-Refresh';
        toggleBtn.style.background = '#dc2626';
      }
    }
  </script>
</body>
</html>
    `);
  } else if (url.pathname === '/image') {
    try {
      const t = url.searchParams.get('t') || String(Date.now());
      const selector = new Selector('jot/webcam/capture', { t });
      const result = await vfs.readSelector(selector);

      // Trigger background pruning to prevent disk bloat
      pruneStorage();

      if (result && result.stream) {
        res.writeHead(200, {
          'Content-Type': 'image/jpeg',
          'Cache-Control': 'no-cache, no-store, must-revalidate',
          'Pragma': 'no-cache',
          'Expires': '0'
        });
        await pipeline(Readable.fromWeb(result.stream), res);
      } else {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Not Found');
      }
    } catch (err) {
      console.error('[Webcam Server] Error reading VFS Selector:', err);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end('Failed to retrieve image from VFS.');
      }
    }
  } else if (url.pathname.startsWith('/vfs')) {
    if (vfsHandlers.request) {
      vfsHandlers.request(req, res);
    } else {
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end('VFS routes not registered.');
    }
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
  }
};

if (process.env.DISABLE_SSL !== '1' && fs.existsSync(keyPath) && fs.existsSync(certPath)) {
  console.log(`[Webcam Node ${id}] SSL certificates found. Starting in HTTPS mode...`);
  const options = {
    key: fs.readFileSync(keyPath),
    cert: fs.readFileSync(certPath)
  };
  server = https.createServer(options, requestHandler);
  protocol = 'https';
} else {
  console.log(`[Webcam Node ${id}] SSL disabled or certificates missing. Starting in HTTP mode...`);
  server = http.createServer(requestHandler);
  protocol = 'http';
}

// 4. Initialize MeshLink if neighbors exist
let meshLink = null;
if (neighbors.length > 0) {
  console.log(`[Webcam Node ${id}] Starting MeshLink with neighbors: [${neighbors.join(', ')}]`);
  meshLink = new MeshLink(vfs, neighbors, { localUrl: `${protocol}://localhost:${PORT}` });
}

// 5. Set up the VFS REST routes using a mock server to intercept request/upgrade events
const vfsHandlers = {};
const mockServer = {
  on(event, handler) {
    vfsHandlers[event] = handler;
  },
  off(event, handler) {
    delete vfsHandlers[event];
  }
};
registerVFSRoutes(vfs, mockServer, '/vfs', meshLink);

// 6. Forward Upgrade requests for WebSocket connection tunnel support
server.on('upgrade', (request, socket, head) => {
  const pathname = new URL(request.url, `${protocol}://${request.headers.host || 'localhost'}`).pathname;
  if (pathname.startsWith('/vfs') && vfsHandlers.upgrade) {
    vfsHandlers.upgrade(request, socket, head);
  } else {
    socket.destroy();
  }
});

// 7. Listen on all network interfaces
server.listen(PORT, '0.0.0.0', async () => {
  console.log(`Webcam VFS Server running on ${protocol}://0.0.0.0:${PORT}`);
  console.log(`Webcam operation registered at VFS Selector path: 'jot/webcam/capture'`);
  console.log(`HTTP VFS Routes exposed under: ${protocol}://0.0.0.0:${PORT}/vfs`);
  if (meshLink) {
    await meshLink.start();
  }
});
