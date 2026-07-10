import http from 'http';
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

// 2. Track latest frames and active HTTP streaming clients
let latestLocalFrame = null;
let latestRemoteFrame = null;

const localClients = new Set();
const remoteClients = new Set();

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

// Subscribe to local and remote camera feeds published on Zenoh Pub/Sub
vfs.listen('jot/webcam/feed/live_webcam_webcam', (payload) => {
  latestLocalFrame = payload;
  pushFrameToClients(localClients, payload);
});

vfs.listen('jot/webcam/feed/webcam-jh', (payload) => {
  latestRemoteFrame = payload;
  pushFrameToClients(remoteClients, payload);
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
  </style>
</head>
<body>
  <div class="header">
    <h1>Dual Camera Monitor</h1>
    <p>Real-time side-by-side VFS webcam feeds (via Zenoh Pub/Sub)</p>
  </div>

  <div class="grid-container">
    <div class="camera-card">
      <div class="card-header">
        <h3 class="card-title">Local Camera</h3>
        <div id="badge-local" class="status-badge">
          <span class="status-dot"></span>Live
        </div>
      </div>
      <div class="feed-container">
        <img id="feed-local" src="/stream/local" onerror="handleError(this, 'badge-local')" alt="Local Feed" />
      </div>
    </div>

    <div class="camera-card">
      <div class="card-header">
        <h3 class="card-title">Remote Camera (jh)</h3>
        <div id="badge-remote" class="status-badge">
          <span class="status-dot"></span>Live
        </div>
      </div>
      <div class="feed-container">
        <img id="feed-remote" src="/stream/remote" onerror="handleError(this, 'badge-remote')" alt="Remote Feed" />
      </div>
    </div>
  </div>

  <script>
    function handleError(img, badgeId) {
      const badge = document.getElementById(badgeId);
      badge.className = 'status-badge offline';
      badge.innerHTML = '<span class=\"status-dot\"></span>Offline';
      
      // Attempt reconnection every 5 seconds
      setTimeout(() => {
        const path = img.src.split('?')[0];
        img.src = path + '?t=' + Date.now();
        badge.className = 'status-badge';
        badge.innerHTML = '<span class=\"status-dot\"></span>Live';
      }, 5000);
    }
  </script>
</body>
</html>
`;

const server = http.createServer((req, res) => {
  if (req.url === '/' || req.url === '/index.html') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(htmlContent);
  } else if (req.url === '/stream/local') {
    handleStreamRequest(res, localClients, latestLocalFrame);
  } else if (req.url === '/stream/remote') {
    handleStreamRequest(res, remoteClients, latestRemoteFrame);
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
  }
});

server.listen(PORT, '0.0.0.0', () => {
  console.log(`[Dual UX] Server running on http://0.0.0.0:${PORT} (all interfaces)`);
});
