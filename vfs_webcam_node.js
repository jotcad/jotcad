import http from 'http';
import https from 'https';
import fs from 'fs';
import { spawn, exec } from 'child_process';
import { Readable } from 'stream';
import { pipeline } from 'stream/promises';
import { promisify } from 'util';
import path from 'path';
const execAsync = promisify(exec);
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

async function getFrameCount(videoPath) {
  try {
    const { stdout } = await execAsync(`ffprobe -v error -select_streams v:0 -count_packets -show_entries stream=nb_read_packets -of csv=p=0 "${videoPath}"`);
    const count = parseInt(stdout.trim(), 10);
    return isNaN(count) ? 0 : count;
  } catch (e) {
    console.error(`[ffprobe] Failed to get frame count for ${videoPath}:`, e);
    return 0;
  }
}

// Helper to compile a specific hour directory into hour_HH.mp4
async function compileHourVideo(dateStr, hourStr) {
  const hourDir = path.join(timelapseDir, dateStr, hourStr);
  const hourVideoPath = path.join(timelapseDir, dateStr, `hour_${hourStr}.mp4`);

  if (!fs.existsSync(hourDir)) return;

  // Clean 0-byte files in the folder
  try {
    const files = fs.readdirSync(hourDir);
    for (const file of files) {
      const filePath = path.join(hourDir, file);
      const stats = fs.statSync(filePath);
      if (stats.size === 0) {
        fs.unlinkSync(filePath);
      }
    }
  } catch (e) {}

  const files = fs.readdirSync(hourDir).filter(f => f.match(/^frame_\d+\.jpg$/));
  if (files.length === 0) {
    // Empty directory, just remove it
    fs.rmSync(hourDir, { recursive: true, force: true });
    return;
  }

  console.log(`[Timelapse] Pre-compiling hourly video for ${dateStr} ${hourStr}:00 (${files.length} frames)...`);

  await new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', [
      '-y',
      '-framerate', '5',
      '-pattern_type', 'glob',
      '-i', path.join(hourDir, 'frame_*.jpg'),
      '-vf', 'split=2[full][masked];[masked]drawbox=x=0:y=0:w=450:h=60:color=black:t=fill,mpdecimate=hi=64*20:lo=64*10:frac=0.4[deduped];[deduped][full]overlay=shortest=1,setpts=N/5/TB',
      '-r', '5',
      '-c:v', 'libx264',
      '-pix_fmt', 'yuv420p',
      '-g', '1',
      '-movflags', '+faststart',
      hourVideoPath
    ]);

    ffmpeg.on('close', (code) => {
      if (code === 0) {
        console.log(`[Timelapse] Successfully compiled hourly video: hour_${hourStr}.mp4`);
        resolve();
      } else {
        reject(new Error(`ffmpeg hourly exited with code ${code}`));
      }
    });

    ffmpeg.on('error', reject);
  });

  // Get the true frame count from ffprobe (after mpdecimate has dropped duplicates)
  const trueCount = await getFrameCount(hourVideoPath);

  // Write frame count metadata to a json file
  fs.writeFileSync(hourVideoPath + '.json', JSON.stringify({ count: trueCount }));

  // Delete source JPEGs and directory to save space
  fs.rmSync(hourDir, { recursive: true, force: true });
  console.log(`[Timelapse] Cleaned up source JPEG folder: ${dateStr}/${hourStr}/ (Output: ${trueCount}/${files.length} frames)`);
}

// Helper to compile a past completed day folder into a single daily video
async function compileDailyVideo(dateStr) {
  const datePath = path.join(timelapseDir, dateStr);
  const dailyVideoPath = path.join(timelapseDir, `${dateStr}.mp4`);

  if (fs.existsSync(dailyVideoPath)) {
    // Already compiled
    return;
  }

  if (!fs.existsSync(datePath)) return;

  const files = fs.readdirSync(datePath)
    .filter(f => f.match(/^hour_\d{2}\.mp4$/))
    .sort();

  if (files.length === 0) {
    // Clean up empty directory if any
    fs.rmSync(datePath, { recursive: true, force: true });
    return;
  }

  console.log(`[Timelapse] Compiling daily video for ${dateStr} from ${files.length} hourly segments...`);

  const segmentPaths = files.map(f => path.join(datePath, f));
  const dailyConcatListPath = path.join(datePath, 'daily_concat_list.txt');

  if (segmentPaths.length === 1) {
    fs.copyFileSync(segmentPaths[0], dailyVideoPath);
  } else {
    const listContent = segmentPaths.map(p => `file '${p.replace(/'/g, "'\\''")}'`).join('\n');
    fs.writeFileSync(dailyConcatListPath, listContent);

    await new Promise((resolve, reject) => {
      const ffmpeg = spawn('ffmpeg', [
        '-y',
        '-f', 'concat',
        '-safe', '0',
        '-i', dailyConcatListPath,
        '-c', 'copy',
        '-movflags', '+faststart',
        dailyVideoPath
      ]);
      ffmpeg.on('close', (code) => {
        if (code === 0) resolve();
        else reject(new Error(`ffmpeg daily concat exited with code ${code}`));
      });
      ffmpeg.on('error', reject);
    });
  }

  // Delete source directory and files to save space
  fs.rmSync(datePath, { recursive: true, force: true });
  console.log(`[Timelapse] Successfully compiled daily video: ${dateStr}.mp4 and cleaned up folder.`);
}

// Helper to compile the today's partial timelapse video on demand
async function compileVideo() {
  const tempActivePath = path.join(timelapseDir, 'temp_active_hour.mp4');
  const concatListPath = path.join(timelapseDir, 'concat_list.txt');
  const outputPath = path.join(timelapseDir, 'timelapse.mp4');

  try {
    const now = new Date();
    const activeDayStr = now.toISOString().split('T')[0];
    const activeHourStr = String(now.getHours()).padStart(2, '0');

    // 0. Auto-backfill any past completed hour JPEG directories that haven't been compiled yet
    try {
      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });

      for (const dateDir of dateDirs) {
        const datePath = path.join(timelapseDir, dateDir);
        const hourDirs = fs.readdirSync(datePath).filter(f => {
          const fullPath = path.join(datePath, f);
          return fs.statSync(fullPath).isDirectory() && f.match(/^\d{2}$/);
        });

        for (const hourDir of hourDirs) {
          if (dateDir !== activeDayStr || hourDir !== activeHourStr) {
            await compileHourVideo(dateDir, hourDir).catch(err => {
              console.error(`[Timelapse Rebuild] Failed to backfill compilation for past hour ${dateDir}/${hourDir}:`, err);
            });
          }
        }

        // Compile past day into YYYY-MM-DD.mp4 if it is a completed past day
        if (dateDir !== activeDayStr) {
          await compileDailyVideo(dateDir).catch(err => {
            console.error(`[Timelapse Rebuild] Failed to compile daily video for past day ${dateDir}:`, err);
          });
        }
      }
    } catch (e) {
      console.error('[Timelapse Rebuild] On-demand backfill failed:', e);
    }

    // 1. Compile the active hour's JPEGs to a temporary video
    const activeHourDir = path.join(timelapseDir, activeDayStr, activeHourStr);

    let hasActiveFrames = false;
    if (fs.existsSync(activeHourDir)) {
      // Clean 0-byte files in active folder
      try {
        const files = fs.readdirSync(activeHourDir);
        for (const file of files) {
          const filePath = path.join(activeHourDir, file);
          const stats = fs.statSync(filePath);
          if (stats.size === 0) {
            fs.unlinkSync(filePath);
          }
        }
      } catch (e) {}

      const activeJpegs = fs.readdirSync(activeHourDir).filter(f => f.match(/^frame_\d+\.jpg$/));
      if (activeJpegs.length > 0) {
        console.log(`[Timelapse Rebuild] Compiling temporary active hour video (${activeJpegs.length} frames)...`);
        await new Promise((resolve, reject) => {
          const ffmpeg = spawn('ffmpeg', [
            '-y',
            '-framerate', '5',
            '-pattern_type', 'glob',
            '-i', path.join(activeHourDir, 'frame_*.jpg'),
            '-vf', 'split=2[full][masked];[masked]drawbox=x=0:y=0:w=450:h=60:color=black:t=fill,mpdecimate=hi=64*20:lo=64*10:frac=0.4[deduped];[deduped][full]overlay=shortest=1,setpts=N/5/TB',
            '-r', '5',
            '-c:v', 'libx264',
            '-pix_fmt', 'yuv420p',
            '-g', '1',
            '-movflags', '+faststart',
            tempActivePath
          ]);
          ffmpeg.on('close', (code) => {
            if (code === 0) resolve();
            else reject(new Error(`ffmpeg active hour compile failed with code ${code}`));
          });
          ffmpeg.on('error', reject);
        });
        hasActiveFrames = true;
      }
    }

    // 2. Gather only today's hourly videos in chronological order
    const videoSegments = [];
    const todayPath = path.join(timelapseDir, activeDayStr);
    if (fs.existsSync(todayPath)) {
      const files = fs.readdirSync(todayPath)
        .filter(f => f.match(/^hour_\d{2}\.mp4$/))
        .sort();
      for (const file of files) {
        videoSegments.push(path.join(todayPath, file));
      }
    }

    if (hasActiveFrames) {
      videoSegments.push(tempActivePath);
    }

    if (videoSegments.length === 0) {
      throw new Error("No active frames or hourly segments collected yet for today.");
    }

    console.log(`[Timelapse Rebuild] Concatenating ${videoSegments.length} segments for today (${activeDayStr})...`);

    // 3. If there is only one segment, copy it directly to outputPath
    if (videoSegments.length === 1) {
      fs.copyFileSync(videoSegments[0], outputPath);
    } else {
      const listContent = videoSegments.map(p => `file '${p.replace(/'/g, "'\\''")}'`).join('\n');
      fs.writeFileSync(concatListPath, listContent);

      await new Promise((resolve, reject) => {
        const ffmpeg = spawn('ffmpeg', [
          '-y',
          '-f', 'concat',
          '-safe', '0',
          '-i', concatListPath,
          '-c', 'copy',
          '-movflags', '+faststart',
          outputPath
        ]);
        ffmpeg.on('close', (code) => {
          if (code === 0) resolve();
          else reject(new Error(`ffmpeg concat failed with code ${code}`));
        });
        ffmpeg.on('error', reject);
      });
    }

    console.log('[Timelapse Rebuild] Today\'s partial video successfully compiled.');
  } finally {
    // Clean up temporary files
    if (fs.existsSync(tempActivePath)) {
      fs.rmSync(tempActivePath, { force: true });
    }
    if (fs.existsSync(concatListPath)) {
      fs.rmSync(concatListPath, { force: true });
    }
  }
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
    .daily-videos-section {
      background: #1a1a1e;
      padding: 20px;
      border-radius: 12px;
      box-shadow: 0 4px 20px rgba(0,0,0,0.6);
      max-width: 1300px;
      width: 100%;
      margin-top: 30px;
      box-sizing: border-box;
    }
    .video-list {
      display: grid;
      grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
      gap: 20px;
      margin-top: 15px;
    }
    .video-card {
      background: #26262b;
      border-radius: 8px;
      padding: 15px;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 10px;
      border: 1px solid #323239;
    }
    .video-card-title {
      font-weight: 500;
      color: #e1e1e6;
    }
    .video-card a {
      color: #60a5fa;
      text-decoration: none;
      font-size: 14px;
      font-weight: 500;
    }
    .video-card a:hover {
      text-decoration: underline;
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

  <div class="daily-videos-section">
    <h3>Completed Daily Timelapses</h3>
    <div id="daily-video-list" class="video-list">
      <p style="color: #98a5b9; grid-column: 1/-1; text-align: center;">Loading completed daily timelapses...</p>
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

    async function loadDailyVideos() {
      const list = document.getElementById('daily-video-list');
      try {
        const resp = await fetch('/timelapse/list');
        if (resp.ok) {
          const data = await resp.json();
          if (data.videos.length === 0) {
            list.innerHTML = '<p style="color: #98a5b9; grid-column: 1/-1; text-align: center;">No completed daily timelapses yet.</p>';
            return;
          }
          list.innerHTML = data.videos.map(v => \`
            <div class="video-card">
              <div class="video-card-title">\${v.replace('.mp4', '')}</div>
              <video controls style="max-height: 160px; border-radius: 4px; background: #000; width: 100%;">
                <source src="/timelapse/\${v}" type="video/mp4">
              </video>
              <a href="/timelapse/\${v}" download="\${v}">Download MP4</a>
            </div>
          \`).join('');
        }
      } catch (e) {
        console.error('Failed to load daily videos:', e);
        list.innerHTML = '<p style="color: #dc2626; grid-column: 1/-1; text-align: center;">Failed to load daily timelapses.</p>';
      }
    }

    // Initialize UI data
    updateFrameCount();
    setInterval(updateFrameCount, 5000);
    loadDailyVideos();
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
      const now = new Date();
      const activeDayStr = now.toISOString().split('T')[0];
      const datePath = path.join(timelapseDir, activeDayStr);
      let count = 0;
      if (fs.existsSync(datePath)) {
        const subfiles = await fs.promises.readdir(datePath).catch(() => []);
        // Add counts from all JSON metadata files of today
        for (const subfile of subfiles) {
          if (subfile.endsWith('.mp4.json')) {
            try {
              const meta = JSON.parse(fs.readFileSync(path.join(datePath, subfile), 'utf8'));
              count += meta.count || 0;
            } catch (e) {}
          }
        }
        // Also count JPEGs in active hour directories of today
        const hourDirs = subfiles.filter(f => {
          const fp = path.join(datePath, f);
          return fs.statSync(fp).isDirectory() && f.match(/^\d{2}$/);
        });
        for (const hourDir of hourDirs) {
          const jpegs = await fs.promises.readdir(path.join(datePath, hourDir)).catch(() => []);
          count += jpegs.filter(f => f.match(/^frame_\d+\.jpg$/)).length;
        }
      }
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate'
      });
      res.end(JSON.stringify({ count }));
    } catch (err) {
      console.error('[Timelapse Count] Error:', err);
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
  } else if (url.pathname === '/timelapse/list') {
    try {
      const files = await fs.promises.readdir(timelapseDir).catch(() => []);
      const dailyVideos = files
        .filter(f => f.match(/^\d{4}-\d{2}-\d{2}\.mp4$/))
        .sort()
        .reverse(); // Reverse chronological order (latest day first)
      res.writeHead(200, {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate'
      });
      res.end(JSON.stringify({ videos: dailyVideos }));
    } catch (err) {
      console.error('[Timelapse List] Error:', err);
      res.writeHead(500, { 'Content-Type': 'text/plain' });
      res.end('Error retrieving daily video list.');
    }
  } else if (url.pathname.match(/^\/timelapse\/\d{4}-\d{2}-\d{2}\.mp4$/)) {
    const filename = path.basename(url.pathname);
    const videoPath = path.join(timelapseDir, filename);

    try {
      if (!fs.existsSync(videoPath)) {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Daily video not found');
        return;
      }

      const stat = await fs.promises.stat(videoPath);
      const fileSize = stat.size;
      const range = req.headers.range;

      if (range) {
        const parts = range.replace(/bytes=/, "").split("-");
        const start = parseInt(parts[0], 10);
        const end = parts[1] ? parseInt(parts[1], 10) : fileSize - 1;

        if (start >= fileSize || end >= fileSize) {
          res.writeHead(416, {
            'Content-Range': `bytes */${fileSize}`,
            'Content-Type': 'text/plain'
          });
          res.end('Requested range not satisfiable.');
          return;
        }

        const chunksize = (end - start) + 1;
        res.writeHead(206, {
          'Content-Range': `bytes ${start}-${end}/${fileSize}`,
          'Accept-Ranges': 'bytes',
          'Content-Length': chunksize,
          'Content-Type': 'video/mp4',
          'Cache-Control': 'no-cache, no-store, must-revalidate'
        });

        const readStream = fs.createReadStream(videoPath, { start, end });
        readStream.pipe(res);
      } else {
        res.writeHead(200, {
          'Content-Length': fileSize,
          'Content-Type': 'video/mp4',
          'Accept-Ranges': 'bytes',
          'Cache-Control': 'no-cache, no-store, must-revalidate'
        });
        const readStream = fs.createReadStream(videoPath);
        readStream.pipe(res);
      }
    } catch (err) {
      console.error('[Daily Video Stream] Error:', err);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end(err.message);
      }
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

    // 1. Group any loose frame_*.jpg in YYYY-MM-DD directories into hourly subdirectories based on their mtime
    try {
      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });

      for (const dateDir of dateDirs) {
        const datePath = path.join(timelapseDir, dateDir);
        const files = fs.readdirSync(datePath);
        for (const file of files) {
          if (file.match(/^frame_\d+\.jpg$/)) {
            const filePath = path.join(datePath, file);
            const stats = fs.statSync(filePath);
            const date = new Date(stats.mtimeMs);
            const hourStr = String(date.getHours()).padStart(2, '0');
            const hourDir = path.join(datePath, hourStr);
            if (!fs.existsSync(hourDir)) {
              fs.mkdirSync(hourDir, { recursive: true });
            }
            fs.renameSync(filePath, path.join(hourDir, file));
            console.log(`[Timelapse Migration] Moved ${dateDir}/${file} to hourly folder ${hourStr}/`);
          }
        }
      }
    } catch (e) {
      console.error('[Timelapse] Startup frame grouping failed:', e);
    }

    // 2. Compile any past completed hourly folders into MP4 files
    try {
      const now = new Date();
      const currentDayStr = now.toISOString().split('T')[0];
      const currentHourStr = String(now.getHours()).padStart(2, '0');

      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });

      for (const dateDir of dateDirs) {
        const datePath = path.join(timelapseDir, dateDir);
        const hourDirs = fs.readdirSync(datePath).filter(f => {
          const fullPath = path.join(datePath, f);
          return fs.statSync(fullPath).isDirectory() && f.match(/^\d{2}$/);
        });

        for (const hourDir of hourDirs) {
          if (dateDir !== currentDayStr || hourDir !== currentHourStr) {
            await compileHourVideo(dateDir, hourDir).catch(err => {
              console.error(`[Timelapse] Failed to compile past hour ${dateDir}/${hourDir}:`, err);
            });
          }
        }
      }
    } catch (e) {
      console.error('[Timelapse] Startup hourly pre-compilation failed:', e);
    }

    // 3. Compile any past completed day folders into YYYY-MM-DD.mp4
    try {
      const now = new Date();
      const currentDayStr = now.toISOString().split('T')[0];
      const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
        const fullPath = path.join(timelapseDir, f);
        return fs.statSync(fullPath).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
      });

      for (const dateDir of dateDirs) {
        if (dateDir !== currentDayStr) {
          await compileDailyVideo(dateDir).catch(err => {
            console.error(`[Timelapse] Failed to backfill daily video for past day ${dateDir}:`, err);
          });
        }
      }
    } catch (e) {
      console.error('[Timelapse] Startup daily pre-compilation failed:', e);
    }

    // Keep track of active hour and its frameIndex
    let activeDayStr = new Date().toISOString().split('T')[0];
    let activeHourStr = String(new Date().getHours()).padStart(2, '0');
    let frameIndex = 1;

    // Find next index for the current hour
    try {
      const currentHourDir = path.join(timelapseDir, activeDayStr, activeHourStr);
      if (fs.existsSync(currentHourDir)) {
        const files = fs.readdirSync(currentHourDir).filter(f => f.match(/^frame_\d+\.jpg$/));
        const numbers = files
          .map(f => {
            const match = f.match(/^frame_(\d+)\.jpg$/);
            return match ? parseInt(match[1], 10) : 0;
          })
          .filter(n => n > 0);
        if (numbers.length > 0) {
          frameIndex = Math.max(...numbers) + 1;
        }
      }
    } catch (e) {}

    console.log(`[Timelapse] Active hour: ${activeDayStr} ${activeHourStr}:00. Next frame index: ${frameIndex}`);

    setInterval(async () => {
      try {
        const now = new Date();
        const currentDayStr = now.toISOString().split('T')[0];
        const currentHourStr = String(now.getHours()).padStart(2, '0');

        // Check for hourly rollover
        if (currentDayStr !== activeDayStr || currentHourStr !== activeHourStr) {
          console.log(`[Timelapse] Hour rollover detected from ${activeDayStr} ${activeHourStr}:00 to ${currentDayStr} ${currentHourStr}:00`);
          const oldDayStr = activeDayStr;
          const oldHourStr = activeHourStr;

          // Reset active hour properties
          activeDayStr = currentDayStr;
          activeHourStr = currentHourStr;
          frameIndex = 1;

          // Compile the completed hour in background
          compileHourVideo(oldDayStr, oldHourStr).then(async () => {
            if (oldDayStr !== currentDayStr) {
              await compileDailyVideo(oldDayStr).catch(err => {
                console.error(`[Timelapse] Failed to compile daily video for rolled-over day ${oldDayStr}:`, err);
              });
            }
          }).catch(err => {
            console.error(`[Timelapse] Error compiling finished hour ${oldDayStr}/${oldHourStr}:`, err);
          });
        }

        const t = Date.now();
        const selector = new Selector('jot/webcam/capture', { t });
        const result = await vfs.readSelector(selector);
        
        pruneStorage();

        if (result && result.stream) {
          const targetDir = path.join(timelapseDir, activeDayStr, activeHourStr);
          if (!fs.existsSync(targetDir)) {
            fs.mkdirSync(targetDir, { recursive: true });
          }
          const filename = `frame_${String(frameIndex).padStart(5, '0')}.jpg`;
          const filePath = path.join(targetDir, filename);
          await pipeline(Readable.fromWeb(result.stream), fs.createWriteStream(filePath));
          console.log(`[Timelapse] Saved frame: ${activeDayStr}/${activeHourStr}/${filename}`);
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
