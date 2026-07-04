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
const timelapseDir = path.resolve('./timelapse');

// 1. Initialize the VFS instance
const storageDir = path.resolve(`.vfs_storage_${id}`);
const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();

// Mutex lock to serialize webcam access and prevent simultaneous ffmpeg processes from colliding on /dev/video0
let activeCapturePromise = null;

function captureFrameDirect() {
  return new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', [
      '-f', 'v4l2',
      '-i', DEVICE,
      '-vf', "drawtext=fontfile=/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf:text='%{localtime\\:%Y-%m-%d %H-%M-%S}':fontcolor=white:fontsize=24:box=1:boxcolor=black@0.4:x=10:y=10",
      '-frames:v', '1',
      '-f', 'image2pipe',
      '-vcodec', 'mjpeg',
      '-'
    ]);

    const timeout = setTimeout(() => {
      ffmpeg.kill('SIGKILL');
      reject(new Error('ffmpeg capture timed out after 5000ms'));
    }, 5000);

    const chunks = [];
    ffmpeg.stdout.on('data', (chunk) => {
      chunks.push(chunk);
    });

    ffmpeg.on('close', (code) => {
      clearTimeout(timeout);
      if (code === 0) {
        resolve(Buffer.concat(chunks));
      } else {
        reject(new Error(`ffmpeg exited with code ${code}`));
      }
    });

    ffmpeg.on('error', (err) => {
      clearTimeout(timeout);
      reject(err);
    });
  });
}

async function getFrameSerialized() {
  if (activeCapturePromise) {
    // Return existing capture promise if one is already running
    return activeCapturePromise;
  }

  activeCapturePromise = (async () => {
    try {
      return await captureFrameDirect();
    } finally {
      activeCapturePromise = null;
    }
  })();

  return activeCapturePromise;
}

// 2. Register the VFS provider for the webcam capture operation
vfs.registerProvider('jot/webcam/capture', async (vfsInstance, selector, context) => {
  try {
    const buffer = await getFrameSerialized();
    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(new Uint8Array(buffer));
        controller.close();
      }
    });
    return {
      stream,
      metadata: {
        encoding: 'bytes',
        filename: 'capture.jpg'
      }
    };
  } catch (err) {
    console.error('[Webcam Provider] Failed to capture frame:', err);
    throw err;
  }
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

// Helper to compile the video from frames on demand
async function compileVideo() {
  // Clean up any 0-byte frame files in all date subdirectories to avoid breaking ffmpeg
  const dateDirs = await fs.promises.readdir(timelapseDir).catch(() => []);
  for (const dir of dateDirs) {
    const dirPath = path.join(timelapseDir, dir);
    const stat = await fs.promises.stat(dirPath).catch(() => null);
    if (stat && stat.isDirectory() && dir.match(/^\d{4}-\d{2}-\d{2}$/)) {
      const files = await fs.promises.readdir(dirPath).catch(() => []);
      for (const file of files) {
        if (file.endsWith('.jpg')) {
          const filePath = path.join(dirPath, file);
          const stats = await fs.promises.stat(filePath).catch(() => null);
          if (stats && stats.size === 0) {
            await fs.promises.unlink(filePath).catch(() => {});
          }
        }
      }
    }
  }

  // Count remaining clean files
  const cleanDateDirs = await fs.promises.readdir(timelapseDir).catch(() => []);
  let totalFrames = 0;
  for (const dir of cleanDateDirs) {
    const dirPath = path.join(timelapseDir, dir);
    const stat = await fs.promises.stat(dirPath).catch(() => null);
    if (stat && stat.isDirectory() && dir.match(/^\d{4}-\d{2}-\d{2}$/)) {
      const files = await fs.promises.readdir(dirPath).catch(() => []);
      totalFrames += files.filter(f => f.match(/^frame_\d+\.jpg$/)).length;
    }
  }

  if (totalFrames === 0) {
    throw new Error('No timelapse frames collected yet.');
  }

  console.log(`[Timelapse Video] Recompiling ${totalFrames} frames...`);
  const outputPath = path.join(timelapseDir, 'timelapse.mp4');

  return new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', [
      '-y',
      '-framerate', '5',
      '-pattern_type', 'glob',
      '-i', path.join(timelapseDir, '*', 'frame_*.jpg'),
      '-r', '5',
      '-c:v', 'libx264',
      '-pix_fmt', 'yuv420p',
      '-g', '1',
      '-movflags', '+faststart',
      outputPath
    ]);

    ffmpeg.on('close', (code) => {
      if (code === 0) {
        console.log('[Timelapse Video] Recompilation successful.');
        resolve();
      } else {
        reject(new Error(`ffmpeg exited with code ${code}`));
      }
    });

    ffmpeg.on('error', (err) => {
      reject(err);
    });
  });
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
  <title>JotCAD VFS Webcam & Timelapse Server</title>
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
      margin-bottom: 30px;
      font-size: 28px;
      font-weight: 600;
    }
    .main-layout {
      display: flex;
      gap: 30px;
      flex-wrap: wrap;
      justify-content: center;
      max-width: 1400px;
      width: 100%;
    }
    .video-section {
      display: flex;
      flex-direction: column;
      align-items: center;
      background: #1a1a1e;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.6);
      flex: 1;
      min-width: 340px;
      max-width: 640px;
    }
    h3 {
      margin-top: 0;
      margin-bottom: 15px;
      font-weight: 500;
      color: #98a5b9;
    }
    .video-container {
      background: #000;
      border-radius: 8px;
      overflow: hidden;
      width: 100%;
      aspect-ratio: 4/3;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    img, video {
      width: 100%;
      height: 100%;
      object-fit: contain;
      background: #000;
    }
    .controls {
      margin-top: 20px;
      display: flex;
      gap: 10px;
      width: 100%;
      justify-content: center;
    }
    button {
      color: #fff;
      border: none;
      padding: 10px 20px;
      border-radius: 6px;
      font-size: 14px;
      cursor: pointer;
      font-weight: 500;
      transition: background 0.2s;
    }
  </style>
</head>
<body>
  <h1>VFS Webcam & Timelapse</h1>
  
  <div class="main-layout">
    <div class="video-section">
      <h3>Live Feed</h3>
      <div class="video-container">
        <img id="feed" src="/stream" alt="Webcam Live Feed" />
      </div>
      <div class="controls">
        <button id="btn-stream" onclick="playStream()" style="background: #10b981;">Play Live Video</button>
        <button id="btn-refresh" onclick="refreshSingleFrame()" style="background: #4f46e5;">Capture Single Frame</button>
      </div>
    </div>
    
    <div class="video-section">
      <h3>Timelapse Video (<span id="frame-count">0</span> frames collected)</h3>
      <div class="video-container">
        <video id="timelapse-player" controls>
          <source id="video-src" src="/timelapse.mp4" type="video/mp4">
          Your browser does not support the video tag.
        </video>
      </div>
      <div class="controls">
        <button id="btn-rebuild" onclick="rebuildTimelapse()" style="background: #4f46e5;">Rebuild & Play Timelapse (5 FPS)</button>
      </div>
    </div>
  </div>

  <script>
    const feed = document.getElementById('feed');
    const btnStream = document.getElementById('btn-stream');
    const btnRefresh = document.getElementById('btn-refresh');

    function playStream() {
      feed.src = '/stream';
      btnStream.style.background = '#10b981';
      btnRefresh.style.background = '#4f46e5';
    }

    function refreshSingleFrame() {
      feed.src = '/image?t=' + Date.now();
      btnStream.style.background = '#4f46e5';
      btnRefresh.style.background = '#10b981';
    }

    async function rebuildTimelapse() {
      const player = document.getElementById('timelapse-player');
      const src = document.getElementById('video-src');
      const btn = document.getElementById('btn-rebuild');
      
      btn.innerText = 'Rebuilding...';
      btn.disabled = true;
      btn.style.background = '#eab308'; // yellow loading

      try {
        const resp = await fetch('/timelapse/rebuild');
        if (!resp.ok) {
          throw new Error('Rebuild failed');
        }

        // Force a reload of the video source by appending timestamp
        const newSrc = '/timelapse.mp4?t=' + Date.now();
        src.src = newSrc;
        player.load();
        
        player.oncanplay = () => {
          player.play().catch(e => console.log('Autoplay blocked:', e));
          btn.innerText = 'Rebuild & Play Timelapse (5 FPS)';
          btn.disabled = false;
          btn.style.background = '#4f46e5';
        };
      } catch (e) {
        btn.innerText = 'No Frames Found / Error';
        btn.disabled = false;
        btn.style.background = '#dc2626'; // red error
      }
    }

    async function updateFrameCount() {
      try {
        const resp = await fetch('/timelapse/count');
        if (resp.ok) {
          const data = await resp.json();
          document.getElementById('frame-count').innerText = data.count;
        }
      } catch (e) {
        console.error('Failed to fetch frame count:', e);
      }
    }

    // Periodically update the frame count
    updateFrameCount();
    setInterval(updateFrameCount, 5000);
  </script>
</body>
</html>
    `);
  } else if (url.pathname === '/stream') {
    res.writeHead(200, {
      'Content-Type': 'multipart/x-mixed-replace; boundary=frame',
      'Cache-Control': 'no-cache, no-store, must-revalidate',
      'Connection': 'keep-alive',
      'Pragma': 'no-cache',
      'Expires': '0'
    });

    const streamInterval = setInterval(async () => {
      try {
        const buffer = await getFrameSerialized();
        res.write('--frame\r\n');
        res.write('Content-Type: image/jpeg\r\n');
        res.write(`Content-Length: ${buffer.length}\r\n\r\n`);
        res.write(buffer);
        res.write('\r\n');
      } catch (err) {
        console.error('[Webcam Stream] Frame capture failed:', err);
      }
    }, 1000);

    req.on('close', () => {
      clearInterval(streamInterval);
      console.log('[Webcam Stream] Connection closed by client.');
    });
  } else if (url.pathname === '/image') {
    try {
      const buffer = await getFrameSerialized();

      res.writeHead(200, {
        'Content-Type': 'image/jpeg',
        'Content-Length': buffer.length,
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Pragma': 'no-cache',
        'Expires': '0'
      });
      res.end(buffer);
    } catch (err) {
      console.error('[Webcam Server] Error retrieving image:', err);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end('Failed to retrieve image from VFS.');
      }
    }
  } else if (url.pathname === '/timelapse/count') {
    try {
      const files = await fs.promises.readdir(timelapseDir).catch(() => []);
      let count = 0;
      for (const file of files) {
        const filePath = path.join(timelapseDir, file);
        const stat = await fs.promises.stat(filePath).catch(() => null);
        if (stat && stat.isDirectory() && file.match(/^\d{4}-\d{2}-\d{2}$/)) {
          const subfiles = await fs.promises.readdir(filePath).catch(() => []);
          count += subfiles.filter(f => f.match(/^frame_\d+\.jpg$/)).length;
        }
      }
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate'
      });
      res.end(JSON.stringify({ count }));
    } catch (err) {
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end('Error retrieving count.');
    }
  } else if (url.pathname === '/timelapse/rebuild') {
    try {
      await compileVideo();
      res.writeHead(200, { 'Content-Type': 'application/json' });
      res.end(JSON.stringify({ success: true }));
    } catch (err) {
      console.error('[Timelapse Rebuild] Error:', err);
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end(err.message);
    }
  } else if (url.pathname === '/timelapse.mp4') {
    const outputPath = path.join(timelapseDir, 'timelapse.mp4');

    try {
      if (!fs.existsSync(outputPath)) {
        await compileVideo();
      }

      const stat = await fs.promises.stat(outputPath);
      const fileSize = stat.size;
      const range = req.headers.range;

      console.log(`[Timelapse Stream] Range header: "${range || 'none'}" | File size: ${fileSize}`);

      if (range) {
        const parts = range.replace(/bytes=/, "").split("-");
        const start = parseInt(parts[0], 10);
        const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;

        if (start >= fileSize || end >= fileSize) {
          console.warn(`[Timelapse Stream] Out of range: ${start}-${end} (file size ${fileSize})`);
          res.writeHead(416, {
            'Content-Range': `bytes */${fileSize}`,
            'Content-Type': 'text/plain'
          });
          res.end('Requested range not satisfiable.');
          return;
        }

        const chunksize = (end - start) + 1;
        console.log(`[Timelapse Stream] Responding 206: bytes ${start}-${end}/${fileSize} | Chunk size: ${chunksize}`);
        res.writeHead(206, {
          'Content-Range': `bytes ${start}-${end}/${fileSize}`,
          'Accept-Ranges': 'bytes',
          'Content-Length': chunksize,
          'Content-Type': 'video/mp4',
          'Cache-Control': 'no-cache, no-store, must-revalidate'
        });

        const readStream = fs.createReadStream(outputPath, { start, end });
        readStream.pipe(res);
      } else {
        console.log(`[Timelapse Stream] Responding 200: full file size ${fileSize}`);
        res.writeHead(200, {
          'Content-Length': fileSize,
          'Content-Type': 'video/mp4',
          'Accept-Ranges': 'bytes',
          'Cache-Control': 'no-cache, no-store, must-revalidate'
        });
        const readStream = fs.createReadStream(outputPath);
        readStream.pipe(res);
      }
    } catch (err) {
      console.error('[Timelapse Video] Error:', err);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end(err.message);
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

  // 8. Optional Timelapse Collector Loop
  if (process.env.TIMELAPSE === 'true') {
    const TIMELAPSE_INTERVAL = parseInt(process.env.TIMELAPSE_INTERVAL || '10000', 10); // default 10 seconds

    if (!fs.existsSync(timelapseDir)) {
      fs.mkdirSync(timelapseDir, { recursive: true });
    }

    // Migrate any existing frame_*.jpg files in root timelapseDir to date subfolders on startup based on their mtime
    try {
      const files = fs.readdirSync(timelapseDir);
      for (const file of files) {
        if (file.match(/^frame_\d+\.jpg$/)) {
          const filePath = path.join(timelapseDir, file);
          const stats = fs.statSync(filePath);
          const dateStr = new Date(stats.mtimeMs).toISOString().split('T')[0]; // e.g. YYYY-MM-DD
          const targetDir = path.join(timelapseDir, dateStr);
          if (!fs.existsSync(targetDir)) {
            fs.mkdirSync(targetDir, { recursive: true });
          }
          fs.renameSync(filePath, path.join(targetDir, file));
          console.log(`[Timelapse] Migrated ${file} to ${dateStr}/`);
        }
      }
    } catch (e) {
      console.error('[Timelapse] Migration failed:', e);
    }

    // Find next sequential frame index by checking existing files in all YYYY-MM-DD subdirectories
    let frameIndex = 1;
    try {
      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });
      const frameNumbers = [];
      for (const dir of dateDirs) {
        const files = fs.readdirSync(path.join(timelapseDir, dir));
        const numbers = files
          .map(f => {
            const match = f.match(/^frame_(\d+)\.jpg$/);
            return match ? parseInt(match[1], 10) : 0;
          })
          .filter(n => n > 0);
        frameNumbers.push(...numbers);
      }
      if (frameNumbers.length > 0) {
        frameIndex = Math.max(...frameNumbers) + 1;
      }
    } catch (e) {
      console.error('[Timelapse] Failed to scan timelapse directory:', e);
    }

    console.log(`[Timelapse] Starting collection every ${TIMELAPSE_INTERVAL}ms. Next index: ${frameIndex}`);

    // Pre-clean 0-byte frame files at startup in date subdirectories
    try {
      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });
      for (const dir of dateDirs) {
        const dirPath = path.join(timelapseDir, dir);
        const files = fs.readdirSync(dirPath);
        for (const file of files) {
          if (file.endsWith('.jpg')) {
            const filePath = path.join(dirPath, file);
            const stats = fs.statSync(filePath);
            if (stats.size === 0) {
              fs.unlinkSync(filePath);
            }
          }
        }
      }
    } catch (e) {}

    setInterval(async () => {
      try {
        const t = Date.now();
        const selector = new Selector('jot/webcam/capture', { t });
        const result = await vfs.readSelector(selector);
        
        // Clean up old storage files to prevent bloat
        pruneStorage();

        if (result && result.stream) {
          const dateStr = new Date().toISOString().split('T')[0];
          const targetDir = path.join(timelapseDir, dateStr);
          if (!fs.existsSync(targetDir)) {
            fs.mkdirSync(targetDir, { recursive: true });
          }
          const filename = `frame_${String(frameIndex).padStart(5, '0')}.jpg`;
          const filePath = path.join(targetDir, filename);
          await pipeline(Readable.fromWeb(result.stream), fs.createWriteStream(filePath));
          console.log(`[Timelapse] Saved frame: ${dateStr}/${filename}`);
          frameIndex++;
        } else {
          console.warn('[Timelapse] Failed to capture frame: VFS stream was empty');
        }
      } catch (err) {
        console.error('[Timelapse] Error capturing frame:', err);
      }
    }, TIMELAPSE_INTERVAL);
  }
});
