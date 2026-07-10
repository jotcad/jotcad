import http from 'http';
import https from 'https';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { VFS, DiskStorage, MeshLink } from './fs/src/index.js';

const PORT = 3031;
const __dirname = path.dirname(fileURLToPath(import.meta.url));

// 1. Initialize VFS and MeshLink to join the Zenoh mesh
const storageDir = path.join(__dirname, '.vfs_storage_dual_ux');
const vfs = new VFS(new DiskStorage(storageDir));

// Connect to local Zenoh router (port 9000 is our live/webcam profile's router)
const meshLink = new MeshLink(vfs, ['http://127.0.0.1:9000']);
console.log('[Dual UX] VFS and MeshLink initialized. Connected to Zenoh router at http://127.0.0.1:9000');

// 2. Track latest frames, heartbeats, logs and active HTTP streaming clients
let latestLocalFrame = null;
let latestRemoteFrame = null;

const localClients = new Set();
const remoteClients = new Set();

const telemetry = {
  local: { status: 'offline', lastHeartbeat: 0, logs: [] },
  remote: { status: 'offline', lastHeartbeat: 0, logs: [] }
};

function getLiveTelemetry() {
  const now = Date.now();
  // Mark offline if no heartbeat within 12 seconds
  telemetry.local.status = (now - telemetry.local.lastHeartbeat < 12000) ? 'online' : 'offline';
  telemetry.remote.status = (now - telemetry.remote.lastHeartbeat < 12000) ? 'online' : 'offline';
  return telemetry;
}

// Helper to push frame to HTTP MJPEG clients
function pushFrameToClients(clients, payload) {
  const frame = Buffer.from(payload);
  for (const res of clients) {
    try {
      res.write('--frame\r\n');
      res.write('Content-Type: image/jpeg\r\n');
      res.write(`Content-Length: ${frame.length}\r\n\r\n`);
      res.write(frame);
      res.write('\r\n');
    } catch (err) {
      console.error('[Dual UX] Failed to write frame to client:', err.message);
    }
  }
}

// Subscribe to feeds, status heartbeats, and logs from local & remote cameras
vfs.listen('jot/webcam/feed/live_webcam_webcam', (payload) => {
  latestLocalFrame = payload;
  pushFrameToClients(localClients, payload);
});

vfs.listen('jot/webcam/feed/webcam-jh', (payload) => {
  latestRemoteFrame = payload;
  pushFrameToClients(remoteClients, payload);
});

vfs.listen('jot/webcam/status/live_webcam_webcam', (payload) => {
  telemetry.local.lastHeartbeat = Date.now();
});

vfs.listen('jot/webcam/status/webcam-jh', (payload) => {
  telemetry.remote.lastHeartbeat = Date.now();
});

vfs.listen('jot/webcam/logs/live_webcam_webcam', (payload) => {
  const logMsg = new TextDecoder().decode(payload);
  telemetry.local.logs.push(logMsg);
  if (telemetry.local.logs.length > 20) telemetry.local.logs.shift();
});

vfs.listen('jot/webcam/logs/webcam-jh', (payload) => {
  const logMsg = new TextDecoder().decode(payload);
  telemetry.remote.logs.push(logMsg);
  if (telemetry.remote.logs.length > 20) telemetry.remote.logs.shift();
});

// Setup HTTP response for MJPEG stream requests
function handleStreamRequest(res, clients, currentFrame) {
  res.writeHead(200, {
    'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
    'Cache-Control': 'no-cache, no-store, must-revalidate',
    'Connection': 'keep-alive',
    'Pragma': 'no-cache',
    'Expires': '0'
  });

  // Serve current frame immediately if available
  if (currentFrame) {
    try {
      const frame = Buffer.from(currentFrame);
      res.write('--frame\r\n');
      res.write('Content-Type: image/jpeg\r\n');
      res.write(`Content-Length: ${frame.length}\r\n\r\n`);
      res.write(frame);
      res.write('\r\n');
    } catch (err) {}
  }

  clients.add(res);

  res.on('close', () => {
    clients.delete(res);
  });
}

// 3. Serve Frontend HTML & Streams
const htmlContent = `
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>JotCAD Dual Webcam Dashboard</title>
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Inter:wght@400;500;600;700&display=swap" rel="stylesheet">
  <style>
    body {
      font-family: 'Inter', sans-serif;
      background: radial-gradient(circle at top, #1e1e24 0%, #111115 100%);
      color: #f3f4f6;
      margin: 0;
      padding: 40px 20px;
      display: flex;
      flex-direction: column;
      align-items: center;
      min-height: 100vh;
      box-sizing: border-box;
    }
    .header {
      text-align: center;
      margin-bottom: 40px;
    }
    .header h1 {
      font-size: 32px;
      font-weight: 700;
      background: linear-gradient(135deg, #60a5fa 0%, #34d399 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin: 0 0 10px 0;
    }
    .header p {
      color: #9ca3af;
      font-size: 16px;
      margin: 0;
    }
    .grid-container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(450px, 1fr));
      gap: 30px;
      width: 100%;
      max-width: 1200px;
    }
    .camera-card {
      background: rgba(26, 26, 30, 0.6);
      backdrop-filter: blur(12px);
      -webkit-backdrop-filter: blur(12px);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 16px;
      padding: 24px;
      display: flex;
      flex-direction: column;
      box-shadow: 0 10px 30px rgba(0,0,0,0.5);
      transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1), border-color 0.3s;
    }
    .camera-card:hover {
      transform: translateY(-5px);
      border-color: rgba(96, 165, 250, 0.3);
    }
    .card-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 16px;
    }
    .card-title {
      font-size: 18px;
      font-weight: 600;
      color: #f3f4f6;
      margin: 0;
    }
    .status-badge {
      display: flex;
      align-items: center;
      gap: 6px;
      font-size: 12px;
      font-weight: 600;
      padding: 4px 10px;
      border-radius: 12px;
      background: rgba(52, 211, 153, 0.1);
      color: #34d399;
    }
    .status-badge.offline {
      background: rgba(239, 68, 68, 0.1);
      color: #ef4444;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #34d399;
      box-shadow: 0 0 8px #34d399;
    }
    .status-badge.offline .status-dot {
      background: #ef4444;
      box-shadow: 0 0 8px #ef4444;
    }
    .feed-container {
      background: #09090b;
      border-radius: 12px;
      overflow: hidden;
      aspect-ratio: 4/3;
      display: flex;
      align-items: center;
      justify-content: center;
      border: 1px solid rgba(255, 255, 255, 0.03);
    }
    .feed-container img {
      width: 100%;
      height: 100%;
      object-fit: cover;
      background: #000;
    }
    .log-window {
      margin-top: 20px;
      background: rgba(0, 0, 0, 0.4);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 10px;
      overflow: hidden;
      font-family: 'Courier New', Courier, monospace;
      font-size: 11px;
    }
    .log-header {
      background: rgba(255, 255, 255, 0.03);
      padding: 6px 12px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
      color: #8892b0;
      font-weight: 600;
      letter-spacing: 0.5px;
    }
    .log-content {
      padding: 12px;
      height: 130px;
      overflow-y: auto;
      white-space: pre-wrap;
      color: #5af78e;
      line-height: 1.4;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>Dual Camera Monitor</h1>
    <p>Real-time side-by-side VFS webcam feeds & logs (via Zenoh Pub/Sub)</p>
  </div>

  <div class="grid-container">
    <div class="camera-card">
      <div class="card-header">
        <h3 class="card-title">Local Camera</h3>
        <div id="badge-local" class="status-badge offline">
          <span class="status-dot"></span>Offline
        </div>
      </div>
      <div class="feed-container">
        <img id="feed-local" src="/stream/local" onerror="handleError(this, 'badge-local')" alt="Local Feed" />
      </div>
      <div class="log-window">
        <div class="log-header">console.log logs</div>
        <div id="log-content-local" class="log-content">Waiting for heartbeat logs...</div>
      </div>
    </div>

    <div class="camera-card">
      <div class="card-header">
        <h3 class="card-title">Remote Camera (jh)</h3>
        <div id="badge-remote" class="status-badge offline">
          <span class="status-dot"></span>Offline
        </div>
      </div>
      <div class="feed-container">
        <img id="feed-remote" src="/stream/remote" onerror="handleError(this, 'badge-remote')" alt="Remote Feed" />
      </div>
      <div class="log-window">
        <div class="log-header">console.log logs</div>
        <div id="log-content-remote" class="log-content">Waiting for heartbeat logs...</div>
      </div>
    </div>
  </div>

  <script>
    function handleError(img, badgeId) {
      const badge = document.getElementById(badgeId);
      badge.className = 'status-badge offline';
      badge.innerHTML = '<span class="status-dot"></span>Offline';
      
      // Attempt reconnection every 5 seconds
      setTimeout(() => {
        const path = img.src.split('?')[0];
        img.src = path + '?t=' + Date.now();
      }, 5000);
    }

    async function updateTelemetry() {
      try {
        const res = await fetch('/api/status');
        const data = await res.json();

        // Update Local Status and Logs
        const badgeLocal = document.getElementById('badge-local');
        if (data.local.status === 'online') {
          badgeLocal.className = 'status-badge';
          badgeLocal.innerHTML = '<span class="status-dot"></span>Live';
        } else {
          badgeLocal.className = 'status-badge offline';
          badgeLocal.innerHTML = '<span class="status-dot"></span>Offline';
        }
        const logsLocal = document.getElementById('log-content-local');
        logsLocal.innerHTML = data.local.logs.join('\n') || 'Connected. Awaiting logs...';
        logsLocal.scrollTop = logsLocal.scrollHeight;

        // Update Remote Status and Logs
        const badgeRemote = document.getElementById('badge-remote');
        if (data.remote.status === 'online') {
          badgeRemote.className = 'status-badge';
          badgeRemote.innerHTML = '<span class="status-dot"></span>Live';
        } else {
          badgeRemote.className = 'status-badge offline';
          badgeRemote.innerHTML = '<span class="status-dot"></span>Offline';
        }
        const logsRemote = document.getElementById('log-content-remote');
        logsRemote.innerHTML = data.remote.logs.join('\n') || 'Connected. Awaiting logs...';
        logsRemote.scrollTop = logsRemote.scrollHeight;
      } catch (err) {
        console.error('Failed to fetch status telemetry:', err);
      }
    }

    setInterval(updateTelemetry, 2000);
    updateTelemetry();
  </script>
</body>
</html>
`;

const requestHandler = (req, res) => {
  if (req.url === '/' || req.url === '/index.html') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(htmlContent);
  } else if (req.url === '/api/status') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify(getLiveTelemetry()));
  } else if (req.url === '/stream/local') {
    handleStreamRequest(res, localClients, latestLocalFrame);
  } else if (req.url === '/stream/remote') {
    handleStreamRequest(res, remoteClients, latestRemoteFrame);
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
  }
};

const keyPath = path.resolve('.ssl/localhost-key.pem');
const certPath = path.resolve('.ssl/localhost-cert.pem');

let server;
let protocol = 'http';

if (fs.existsSync(keyPath) && fs.existsSync(certPath)) {
  console.log('[Dual UX] SSL certificates found. Starting in HTTPS mode...');
  const options = {
    key: fs.readFileSync(keyPath),
    cert: fs.readFileSync(certPath)
  };
  server = https.createServer(options, requestHandler);
  protocol = 'https';
} else {
  console.log('[Dual UX] SSL disabled or certificates missing. Starting in HTTP mode...');
  server = http.createServer(requestHandler);
  protocol = 'http';
}

server.listen(PORT, '0.0.0.0', () => {
  console.log(`[Dual UX] Server running on ${protocol}://0.0.0.0:${PORT} (all interfaces)`);
});
