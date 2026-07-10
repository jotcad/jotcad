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

process.env.TZ = 'Asia/Seoul';

const id = process.env.VFS_ID || 'webcam-node';
const PORT = parseInt(process.env.PORT || '8080');
const DEVICE = process.env.WEBCAM_DEVICE || '/dev/video0';
const neighbors = (process.env.NEIGHBORS || '').split(',').filter(Boolean);
const timelapseDir = path.resolve('./timelapse');

// Load .env file variables into process.env at startup
try {
  const envPath = path.resolve('.env');
  if (fs.existsSync(envPath)) {
    const envContent = fs.readFileSync(envPath, 'utf8');
    for (const line of envContent.split('\n')) {
      const trimmed = line.trim();
      if (trimmed && !trimmed.startsWith('#')) {
        const firstEquals = trimmed.indexOf('=');
        if (firstEquals !== -1) {
          const key = trimmed.slice(0, firstEquals).trim();
          const val = trimmed.slice(firstEquals + 1).trim();
          process.env[key] = val;
        }
      }
    }
  }
} catch (e) {
  console.error('[Webcam Node] Failed to load .env file:', e);
}

function getLocalDateStr(date = new Date()) {
  const tzOffset = date.getTimezoneOffset() * 60000;
  return (new Date(date.getTime() - tzOffset)).toISOString().split('T')[0];
}

// Setup log publishing with recursion guard
let vfsInstance = null;
let isPublishingLog = false;
const originalLog = console.log;
const originalWarn = console.warn;
const originalError = console.error;

function publishLog(type, args) {
  const msg = `[${type}] ${args.map(a => typeof a === 'object' ? JSON.stringify(a) : String(a)).join(' ')}`;
  originalLog(msg);
  if (vfsInstance && !vfsInstance.isClosed && !isPublishingLog) {
    isPublishingLog = true;
    vfsInstance.write(`jot/webcam/logs/${id}`, msg)
      .catch(() => {})
      .finally(() => {
        isPublishingLog = false;
      });
  }
}

console.log = (...args) => publishLog('INFO', args);
console.warn = (...args) => publishLog('WARN', args);
console.error = (...args) => publishLog('ERROR', args);

// 1. Initialize the VFS instance
const storageDir = path.resolve(`.vfs_storage_${id}`);
const vfs = new VFS({ id, storage: new DiskStorage(storageDir) });
await vfs.init();
vfsInstance = vfs;

// Periodically publish status heartbeat (online status)
setInterval(() => {
  if (vfsInstance && !vfsInstance.isClosed) {
    vfsInstance.write(`jot/webcam/status/${id}`, 'online').catch(() => {});
  }
}, 5000);

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

function getFrameSignature(filePath) {
  return new Promise((resolve) => {
    const ffmpeg = spawn('ffmpeg', [
      '-i', filePath,
      '-vf', 'scale=8:8,format=gray',
      '-f', 'rawvideo',
      '-'
    ]);
    const chunks = [];
    ffmpeg.stdout.on('data', (chunk) => chunks.push(chunk));
    ffmpeg.on('close', (code) => {
      if (code === 0 && chunks.length > 0) {
        resolve(Buffer.concat(chunks));
      } else {
        resolve(Buffer.alloc(64));
      }
    });
  });
}

function getFrameDistance(sigA, sigB) {
  let diff = 0;
  for (let i = 0; i < 64; i++) {
    diff += Math.abs(sigA[i] - sigB[i]);
  }
  return diff;
}

async function getSignatures(dir, files) {
  const signatures = new Array(files.length);
  const concurrency = 8;
  for (let i = 0; i < files.length; i += concurrency) {
    const chunk = files.slice(i, i + concurrency);
    await Promise.all(chunk.map(async (file, idx) => {
      const actualIdx = i + idx;
      signatures[actualIdx] = await getFrameSignature(path.join(dir, file));
    }));
  }
  return signatures;
}

// Helper to filter transient brightness spikes from frame list
async function filterFrames(dir, files) {
  const signatures = await getSignatures(dir, files);
  const averages = signatures.map(sig => sig.reduce((sum, v) => sum + v, 0) / 64);
  const cleanFiles = [];
  
  for (let i = 0; i < files.length; i++) {
    if (i > 0 && i < files.length - 1) {
      const avgPrev = averages[i - 1];
      const avgCurr = averages[i];
      const avgNext = averages[i + 1];
      
      const diffPrev = avgCurr - avgPrev;
      const diffNext = avgCurr - avgNext;
      
      // Drop frame if it is a transient brightness spike (brighter than both neighbors by > 25 units)
      if (diffPrev > 25 && diffNext > 25) {
        console.log(`[Timelapse Filter] Dropping bright spike frame: ${files[i]} (prev diff: ${diffPrev.toFixed(1)}, next diff: ${diffNext.toFixed(1)})`);
        continue;
      }
    }
    cleanFiles.push(files[i]);
  }

  if (cleanFiles.length === 0 && files.length > 0) {
    cleanFiles.push(files[0]);
  }
  return cleanFiles;
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

  const files = fs.readdirSync(hourDir).filter(f => f.match(/^frame_\d+\.jpg$/)).sort();
  if (files.length === 0) {
    // Empty directory, just remove it
    fs.rmSync(hourDir, { recursive: true, force: true });
    return;
  }

  console.log(`[Timelapse] Pre-compiling hourly video for ${dateStr} ${hourStr}:00 (${files.length} frames)...`);

  // Analyze frame sequences in batch to detect and filter transient glitches
  const cleanFiles = await filterFrames(hourDir, files);

  const cleanDir = path.join(hourDir, 'clean');
  fs.mkdirSync(cleanDir, { recursive: true });
  for (const file of cleanFiles) {
    fs.symlinkSync(path.join(hourDir, file), path.join(cleanDir, file));
  }

  await new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', [
      '-y',
      '-framerate', '5',
      '-pattern_type', 'glob',
      '-i', path.join(cleanDir, 'frame_*.jpg'),
      '-vf', 'split=2[full][masked];[masked]drawbox=x=0:y=0:w=450:h=60:color=black:t=fill,mpdecimate=hi=64*20:lo=64*10:frac=0.4[deduped];[deduped][full]overlay=shortest=1,setpts=N/5/TB,hqdn3d,scale=-2:360',
      '-r', '5',
      '-c:v', 'libx264',
      '-pix_fmt', 'yuv420p',
      '-g', '150',
      '-crf', '30',
      '-tune', 'stillimage',
      '-movflags', '+faststart',
      hourVideoPath
    ]);

    ffmpeg.on('close', (code) => {
      if (code === 0) {
        console.log(`[Timelapse] Successfully compiled hourly video: hour_${hourStr}.mp4 (kept ${cleanFiles.length}/${files.length} frames)`);
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

// Helper to perform focus analysis on a compiled daily video via Gemini API
async function analyzeVideo(dateStr) {
  const apiKey = process.env.GEMINI_API_KEY;
  if (!apiKey) {
    console.log('[Timelapse Analysis] GEMINI_API_KEY environment variable not set. Skipping focus analysis.');
    return;
  }

  const videoPath = path.join(timelapseDir, `${dateStr}.mp4`);
  const analysisPath = path.join(timelapseDir, `${dateStr}_analysis.json`);

  if (!fs.existsSync(videoPath)) {
    console.error(`[Timelapse Analysis] Video file not found: ${videoPath}`);
    return;
  }

  if (fs.existsSync(analysisPath)) {
    console.log(`[Timelapse Analysis] Analysis already exists for ${dateStr}. Skipping.`);
    return;
  }

  console.log(`[Timelapse Analysis] Starting Gemini multimodal focus analysis for ${dateStr}.mp4...`);

  try {
    const stats = fs.statSync(videoPath);
    const fileLength = stats.size;
    const displayName = `${dateStr}.mp4`;

    // 1. Resumable Upload Session Initiation
    const uploadInitUrl = `https://generativelanguage.googleapis.com/upload/v1beta/files?key=${apiKey}`;
    const initResponse = await fetch(uploadInitUrl, {
      method: 'POST',
      headers: {
        'X-Goog-Upload-Protocol': 'resumable',
        'X-Goog-Upload-Command': 'start',
        'X-Goog-Upload-Header-Content-Length': String(fileLength),
        'X-Goog-Upload-Header-Content-Type': 'video/mp4',
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        file: { displayName }
      })
    });

    if (!initResponse.ok) {
      const errorText = await initResponse.text();
      throw new Error(`Failed to initiate Gemini upload: ${initResponse.statusText}. Details: ${errorText}`);
    }

    const uploadUrl = initResponse.headers.get('x-goog-upload-url');
    if (!uploadUrl) {
      throw new Error('x-goog-upload-url header not returned in Gemini init response');
    }

    // 2. Upload Video Bytes
    const videoBuffer = fs.readFileSync(videoPath);
    const uploadResponse = await fetch(uploadUrl, {
      method: 'POST',
      headers: {
        'X-Goog-Upload-Offset': '0',
        'X-Goog-Upload-Command': 'upload, finalize'
      },
      body: videoBuffer
    });

    if (!uploadResponse.ok) {
      const errorText = await uploadResponse.text();
      throw new Error(`Failed to upload video bytes to Gemini: ${uploadResponse.statusText}. Details: ${errorText}`);
    }

    const uploadResult = await uploadResponse.json();
    const fileUri = uploadResult.file.uri;
    const fileId = fileUri.split('/').pop();
    console.log(`[Timelapse Analysis] Video uploaded to Gemini. URI: ${fileUri}, File ID: ${fileId}`);

    // 3. Poll status until file is ACTIVE
    let fileActive = false;
    const maxPolls = 30; // 5 minutes max
    for (let poll = 0; poll < maxPolls; poll++) {
      console.log(`[Timelapse Analysis] Polling file status (attempt ${poll + 1}/${maxPolls})...`);
      const statusResponse = await fetch(`https://generativelanguage.googleapis.com/v1beta/files/${fileId}?key=${apiKey}`);
      if (statusResponse.ok) {
        const statusResult = await statusResponse.json();
        const state = statusResult.state;
        console.log(`[Timelapse Analysis] Current file state: ${state}`);
        if (state === 'ACTIVE') {
          fileActive = true;
          break;
        } else if (state === 'FAILED') {
          throw new Error('Gemini file processing failed');
        }
      }
      await new Promise(r => setTimeout(r, 10000)); // Poll every 10 seconds
    }

    if (!fileActive) {
      throw new Error('Gemini video processing timed out (exceeded 5 minutes)');
    }

    // 4. Generate focus analysis content using gemini-2.5-flash
    const generateUrl = `https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key=${apiKey}`;
    const promptText = `
You are an expert activity and focus analysis assistant. Your task is to analyze the attached timelapse video of a study/work desk and generate a detailed chronological activity transcript, with a focus on measuring active attention and distractions.

### Video Context:
* The video contains a timestamp overlaid in the top-left corner of each frame (Format: YYYY-MM-DD HH:MM:SS). Use these overlay timestamps for all clock times in your report.
* The desk is used for various activities (such as tablet drawing/writing, physical notebook study, or computer usage/gaming facing off-camera to the right).

### Instructions:
1. **Chronological Scan**: Identify the start and end overlay timestamps for every active study or gaming session where the user is present at the desk.
2. **Focus/Distraction Segmentation**: For each active block where the user is at the desk, analyze their gaze direction, posture, and active movement to separate the time into:
   - **Focused Duration**: Time spent actively looking at the work surface/tablet/screen, writing, typing, or playing.
   - **Distracted/Idle Duration**: Time spent checking a phone, looking away, talking, stretching, or leaving the chair briefly.
3. **Qualitative Focus Assessment**: Provide a brief descriptive summary of their posture and engagement during that chunk.
4. **Gaming & Leisure**: Note that gaming is a valid activity; measure focus for it neutrally.
5. **Consolidation**: Consolidate contiguous similar activities into single blocks where possible. Do NOT break study sessions into multiple 1-minute blocks; group them into larger blocks (e.g. 15-30 minutes) to keep the timeline concise and prevent token limit truncation.
6. **Nested Sub-Activities**: For major study and gaming blocks where the user is active at the desk, populate the 'subActivities' array with key micro-events that occurred inside it (e.g. phone checks, stretches, parent check-ins, or blocks of focused study). Group consecutive study stretches into single larger sub-activity blocks rather than minute-by-minute entries.
7. **EXCLUSION MANDATE (CRITICAL)**: Exclude any timeline blocks where nothing is happening (e.g., "Desk Empty" or "Brief Absence") or transient walkbys (like a parent briefly checking the room or a child walking past the camera without sitting down to study). The timeline array MUST ONLY contain blocks where the user is actively working, studying, or playing at the desk.

### Output Format:
You MUST respond with a single valid JSON object following this exact schema:
{
  "totalStudyTimeMinutes": number,
  "totalGamingTimeMinutes": number,
  "totalFocusedTimeMinutes": number,
  "totalDistractedTimeMinutes": number,
  "globalFocusRatioPercent": number,
  "timeline": [
    {
      "timeRange": "string",
      "activityName": "string",
      "totalDurationMinutes": number,
      "focusedDurationMinutes": number,
      "distractedDurationMinutes": number,
      "focusLevel": "High" | "Moderate" | "Low",
      "focusAndPostureDetails": "string",
      "subActivities": [
        {
          "timeRange": "string",
          "activityName": "string",
          "durationMinutes": number,
          "isFocused": boolean,
          "details": "string"
        }
      ]
    }
  ]
}
`.trim();

    const schema = {
      type: "OBJECT",
      properties: {
        totalStudyTimeMinutes: { type: "NUMBER" },
        totalGamingTimeMinutes: { type: "NUMBER" },
        totalFocusedTimeMinutes: { type: "NUMBER" },
        totalDistractedTimeMinutes: { type: "NUMBER" },
        globalFocusRatioPercent: { type: "NUMBER" },
        timeline: {
          type: "ARRAY",
          items: {
            type: "OBJECT",
            properties: {
              timeRange: { type: "STRING" },
              activityName: { type: "STRING" },
              totalDurationMinutes: { type: "NUMBER" },
              focusedDurationMinutes: { type: "NUMBER" },
              distractedDurationMinutes: { type: "NUMBER" },
              focusLevel: { type: "STRING", enum: ["High", "Moderate", "Low"] },
              focusAndPostureDetails: { type: "STRING" },
              subActivities: {
                type: "ARRAY",
                items: {
                  type: "OBJECT",
                  properties: {
                    timeRange: { type: "STRING" },
                    activityName: { type: "STRING" },
                    durationMinutes: { type: "NUMBER" },
                    isFocused: { type: "BOOLEAN" },
                    details: { type: "STRING" }
                  },
                  required: ["timeRange", "activityName", "durationMinutes", "isFocused", "details"]
                }
              }
            },
            required: [
              "timeRange",
              "activityName",
              "totalDurationMinutes",
              "focusedDurationMinutes",
              "distractedDurationMinutes",
              "focusLevel",
              "focusAndPostureDetails",
              "subActivities"
            ]
          }
        }
      },
      required: [
        "totalStudyTimeMinutes",
        "totalGamingTimeMinutes",
        "totalFocusedTimeMinutes",
        "totalDistractedTimeMinutes",
        "globalFocusRatioPercent",
        "timeline"
      ]
    };

    const generateResponse = await fetch(generateUrl, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        contents: [
          {
            parts: [
              { fileData: { fileUri, mimeType: 'video/mp4' } },
              { text: promptText }
            ]
          }
        ],
        generationConfig: {
          responseMimeType: 'application/json',
          responseSchema: schema,
          maxOutputTokens: 8192,
          thinkingConfig: {
            thinkingBudget: 0
          }
        }
      })
    });

    if (!generateResponse.ok) {
      const errorText = await generateResponse.text();
      throw new Error(`Failed to generate content from Gemini: ${generateResponse.statusText}. Details: ${errorText}`);
    }

    const generateResult = await generateResponse.json();
    const rawJson = generateResult.candidates?.[0]?.content?.parts?.[0]?.text;

    if (!rawJson) {
      throw new Error('Gemini returned an empty response candidate');
    }

    // Parse to validate JSON. If it throws, it will fail the analyzeVideo task (no file saved)
    let reportObj;
    try {
      reportObj = JSON.parse(rawJson.trim());
    } catch (parseErr) {
      console.error(`[Timelapse Analysis] JSON parsing failed. Raw response text was:\n${rawJson}`);
      console.error(`[Timelapse Analysis] Full API Response result:`, JSON.stringify(generateResult, null, 2));
      throw parseErr;
    }

    // Write output analysis JSON
    fs.writeFileSync(analysisPath, JSON.stringify(reportObj, null, 2));
    console.log(`[Timelapse Analysis] Successfully saved focus analysis for ${dateStr} to ${analysisPath}`);

    // 5. Clean up file from Gemini workspace
    const deleteResponse = await fetch(`https://generativelanguage.googleapis.com/v1beta/files/${fileId}?key=${apiKey}`, {
      method: 'DELETE'
    });
    if (deleteResponse.ok) {
      console.log(`[Timelapse Analysis] Cleaned up file ${fileId} from Gemini.`);
    } else {
      console.warn(`[Timelapse Analysis] Failed to clean up file ${fileId} from Gemini workspace.`);
    }

  } catch (err) {
    console.error(`[Timelapse Analysis] Error analyzing video for ${dateStr}:`, err);
  }
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

  // Trigger focus analysis in background
  analyzeVideo(dateStr).catch(err => {
    console.error('[Timelapse Analysis] Failed to run focus analysis:', err);
  });
}

// Helper to compile the today's partial timelapse video on demand
async function compileVideo() {
  const tempActivePath = path.join(timelapseDir, 'temp_active_hour.mp4');
  const concatListPath = path.join(timelapseDir, 'concat_list.txt');
  const outputPath = path.join(timelapseDir, 'timelapse.mp4');

  try {
    const now = new Date();
    const activeDayStr = getLocalDateStr(now);
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

      const activeJpegs = fs.readdirSync(activeHourDir).filter(f => f.match(/^frame_\d+\.jpg$/)).sort();
      if (activeJpegs.length > 0) {
        console.log(`[Timelapse Rebuild] Compiling temporary active hour video (${activeJpegs.length} frames)...`);
        const cleanFiles = await filterFrames(activeHourDir, activeJpegs);
        const cleanDir = path.join(activeHourDir, 'clean');
        
        // Recreate clean folder
        fs.rmSync(cleanDir, { recursive: true, force: true });
        fs.mkdirSync(cleanDir, { recursive: true });
        for (const file of cleanFiles) {
          fs.symlinkSync(path.join(activeHourDir, file), path.join(cleanDir, file));
        }

        await new Promise((resolve, reject) => {
          const ffmpeg = spawn('ffmpeg', [
            '-y',
            '-framerate', '5',
            '-pattern_type', 'glob',
            '-i', path.join(cleanDir, 'frame_*.jpg'),
            '-vf', 'split=2[full][masked];[masked]drawbox=x=0:y=0:w=450:h=60:color=black:t=fill,mpdecimate=hi=64*20:lo=64*10:frac=0.4[deduped];[deduped][full]overlay=shortest=1,setpts=N/5/TB,hqdn3d,scale=-2:360',
            '-r', '5',
            '-c:v', 'libx264',
            '-pix_fmt', 'yuv420p',
            '-g', '150',
            '-crf', '30',
            '-tune', 'stillimage',
            '-movflags', '+faststart',
            tempActivePath
          ]);
          ffmpeg.on('close', (code) => {
            // Clean up temporary symlink dir
            fs.rmSync(cleanDir, { recursive: true, force: true });
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
    /* Modal Styles */
    .modal {
      display: none;
      position: fixed;
      z-index: 1000;
      left: 0;
      top: 0;
      width: 100%;
      height: 100%;
      overflow: auto;
      background-color: rgba(0,0,0,0.85);
      backdrop-filter: blur(5px);
    }
    .modal-content {
      background-color: #1a1a1e;
      margin: 5% auto;
      padding: 30px;
      border: 1px solid #323239;
      border-radius: 12px;
      width: 90%;
      max-width: 1000px;
      box-shadow: 0 4px 30px rgba(0, 0, 0, 0.9);
      color: #e1e1e6;
      max-height: 80vh;
      overflow-y: auto;
    }
    .close-btn {
      color: #98a5b9;
      float: right;
      font-size: 30px;
      font-weight: bold;
      cursor: pointer;
      transition: color 0.2s;
    }
    .close-btn:hover {
      color: #fff;
    }
    .timeline-container {
      margin-top: 20px;
      position: relative;
      padding-left: 24px;
      border-left: 2px solid #334155;
    }
    .timeline-item {
      position: relative;
      margin-bottom: 24px;
    }
    .timeline-item::before {
      content: '';
      position: absolute;
      left: -31px;
      top: 4px;
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: #475569;
      border: 2px solid #1a1a1e;
    }
    .timeline-item.active-focus::before {
      background: #10b981;
    }
    .timeline-item.active-distracted::before {
      background: #ef4444;
    }
    .timeline-header {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 14px;
      font-weight: 600;
      color: #fff;
    }
    .timeline-time {
      font-family: monospace;
      color: #94a3b8;
      font-size: 13px;
    }
    .focus-badge {
      font-size: 11px;
      padding: 2px 6px;
      border-radius: 4px;
      font-weight: bold;
      text-transform: uppercase;
    }
    .focus-badge.high {
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
    }
    .focus-badge.moderate {
      background: rgba(245, 158, 11, 0.15);
      color: #fbbf24;
    }
    .focus-badge.low {
      background: rgba(239, 68, 68, 0.15);
      color: #f87171;
    }
    .timeline-details {
      margin-top: 6px;
      color: #cbd5e1;
      font-size: 13px;
      line-height: 1.5;
    }
    .timeline-stats {
      font-size: 12px;
      color: #94a3b8;
      margin-top: 4px;
    }
    .timeline-sub-list {
      margin-top: 8px;
      padding-left: 14px;
      border-left: 1.5px dashed #475569;
    }
    .timeline-sub-item {
      margin-top: 6px;
      font-size: 12px;
      color: #94a3b8;
      position: relative;
    }
    .analysis-btn {
      background: #10b981;
      padding: 8px 16px;
      font-size: 13px;
      margin-top: 5px;
      border-radius: 6px;
      border: none;
      color: white;
      cursor: pointer;
      font-weight: 500;
      transition: background 0.2s;
      width: 100%;
      text-align: center;
    }
    .analysis-btn:hover {
      background: #059669;
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
      <div class="controls" style="flex-direction: column; align-items: center; gap: 8px;">
        <button id="btn-rebuild" onclick="rebuildTimelapse()" style="background: #4f46e5;">Rebuild & Play Timelapse (5 FPS)</button>
        <a id="timelapse-download" href="/timelapse.mp4" download="timelapse.mp4" style="color: #60a5fa; text-decoration: none; font-size: 14px; font-weight: 500; margin-top: 5px;">Download Full Partial MP4</a>
      </div>
    </div>
  </div>

  <div class="daily-videos-section">
    <h3>Completed Daily Timelapses</h3>
    <div id="daily-video-list" class="video-list">
      <p style="color: #98a5b9; grid-column: 1/-1; text-align: center;">Loading completed daily timelapses...</p>
    </div>
  </div>

  <!-- Modal for Focus Analysis -->
  <div id="analysis-modal" class="modal">
    <div class="modal-content">
      <span class="close-btn" onclick="closeModal()">&times;</span>
      <div id="analysis-content">Loading focus analysis...</div>
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
      const dlLink = document.getElementById('timelapse-download');
      
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
        dlLink.href = newSrc;
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

    function renderAnalysisJSON(reportObj) {
      let html = '';
      html += '<h1 style="margin-top: 0; color: #fff; font-size: 24px; font-weight: 600;">Daily Focus & Activity Report</h1>';
      html += '<h3 style="margin-top: 20px; color: #98a5b9;">Daily Focus Summary:</h3>';
      html += '<ul style="padding-left: 20px; line-height: 1.6;">';
      html += '<li><strong>Total Study Time</strong>: ' + (reportObj.totalStudyTimeMinutes ?? 0) + ' minutes</li>';
      html += '<li><strong>Total Gaming Time</strong>: ' + (reportObj.totalGamingTimeMinutes ?? 0) + ' minutes</li>';
      html += '<li><strong>Total Focused Time</strong>: ' + (reportObj.totalFocusedTimeMinutes ?? 0) + ' minutes</li>';
      html += '<li><strong>Total Distracted Time</strong>: ' + (reportObj.totalDistractedTimeMinutes ?? 0) + ' minutes</li>';
      html += '<li><strong>Global Focus-to-Distraction Ratio</strong>: ' + (reportObj.globalFocusRatioPercent ?? 0) + '%</li>';
      html += '</ul>';

      html += '<h3 style="margin-top: 25px; color: #98a5b9;">Activity Timeline:</h3>';
      html += '<div class="timeline-container">';

      if (Array.isArray(reportObj.timeline)) {
        for (let i = 0; i < reportObj.timeline.length; i++) {
          const item = reportObj.timeline[i];
          
          let activeClass = '';
          if (item.focusLevel === 'High') {
            activeClass = ' active-focus';
          } else if (item.focusLevel === 'Low' && item.activityName !== 'Desk Empty') {
            activeClass = ' active-distracted';
          }
          
          let badgeClass = 'low';
          if (item.focusLevel === 'High') badgeClass = 'high';
          else if (item.focusLevel === 'Moderate') badgeClass = 'moderate';
          
          html += '<div class="timeline-item' + activeClass + '">';
          html += '  <div class="timeline-header">';
          html += '    <span class="timeline-time">[' + (item.timeRange ?? '') + ']</span>';
          html += '    <span>' + (item.activityName ?? '') + '</span>';
          html += '    <span class="focus-badge ' + badgeClass + '">' + (item.focusLevel ?? 'Low') + ' Focus</span>';
          html += '  </div>';
          
          html += '  <div class="timeline-stats">';
          html += '    Duration: <strong>' + (item.totalDurationMinutes ?? 0) + 'm</strong> | ';
          html += '    Focused: <span style="color: #34d399; font-weight: 500;">' + (item.focusedDurationMinutes ?? 0) + 'm</span> | ';
          html += '    Distracted: <span style="color: #f87171; font-weight: 500;">' + (item.distractedDurationMinutes ?? 0) + 'm</span>';
          html += '  </div>';
          
          html += '  <div class="timeline-details">' + (item.focusAndPostureDetails ?? '') + '</div>';
          
          if (Array.isArray(item.subActivities) && item.subActivities.length > 0) {
            html += '  <div class="timeline-sub-list">';
            for (let j = 0; j < item.subActivities.length; j++) {
              const sub = item.subActivities[j];
              const subColor = sub.isFocused ? '#34d399' : '#f87171';
              const subTag = sub.isFocused ? '[Focused]' : '[Distracted]';
              
              html += '    <div class="timeline-sub-item">';
              html += '      <span style="font-family: monospace; color: #94a3b8;">' + sub.timeRange + '</span> (' + sub.durationMinutes + 'm) - ';
              html += '      <span style="color: ' + subColor + '; font-weight: 500;">' + subTag + '</span> ';
              html += '      <strong>' + sub.activityName + '</strong>: ' + sub.details;
              html += '    </div>';
            }
            html += '  </div>';
          }
          
          html += '</div>';
        }
      }
      html += '</div>';
      return html;
    }

    async function viewAnalysis(dateStr) {
      const modal = document.getElementById('analysis-modal');
      const content = document.getElementById('analysis-content');
      modal.style.display = 'block';
      content.innerHTML = '<h3>Loading analysis for ' + dateStr + '...</h3>';
      try {
        const resp = await fetch('/timelapse/' + dateStr + '_analysis.json');
        if (resp.ok) {
          const reportObj = await resp.json();
          content.innerHTML = renderAnalysisJSON(reportObj);
        } else {
          content.innerHTML = '<p style="color: #dc2626;">Analysis file not found.</p>';
        }
      } catch (e) {
        content.innerHTML = '<p style="color: #dc2626;">Error loading analysis.</p>';
      }
    }

    function closeModal() {
      document.getElementById('analysis-modal').style.display = 'none';
    }

    window.onclick = function(event) {
      const modal = document.getElementById('analysis-modal');
      if (event.target === modal) {
        modal.style.display = 'none';
      }
    }

    async function loadDailyVideos() {
      const list = document.getElementById('daily-video-list');
      console.log('[Webcam UI] loadDailyVideos starting fetch...');
      try {
        const resp = await fetch('/timelapse/list');
        console.log('[Webcam UI] fetch(/timelapse/list) response status:', resp.status);
        if (resp.ok) {
          const data = await resp.json();
          console.log('[Webcam UI] Daily videos data loaded:', data);
          if (data.videos.length === 0) {
            list.innerHTML = '<p style="color: #98a5b9; grid-column: 1/-1; text-align: center;">No completed daily timelapses yet.</p>';
            return;
          }
          list.innerHTML = data.videos.map(v => {
            const dateStr = v.name.replace('.mp4', '');
            const analysisBtn = v.hasAnalysis 
              ? '<button class="analysis-btn" data-date="' + dateStr + '">View Focus Analysis</button>'
              : '';
            return '<div class="video-card">' +
              '<div class="video-card-title">' + dateStr + '</div>' +
              '<video controls style="max-height: 160px; border-radius: 4px; background: #000; width: 100%;">' +
                '<source src="/timelapse/' + v.name + '" type="video/mp4">' +
              '</video>' +
              '<a href="/timelapse/' + v.name + '" download="' + v.name + '">Download MP4</a>' +
              analysisBtn +
            '</div>';
          }).join('');
        } else {
          list.innerHTML = '<p style="color: #dc2626; grid-column: 1/-1; text-align: center;">Failed to load daily timelapses (HTTP ' + resp.status + ').</p>';
        }
      } catch (e) {
        console.error('[Webcam UI] Failed to load daily videos:', e);
        list.innerHTML = '<p style="color: #dc2626; grid-column: 1/-1; text-align: center;">Failed to load daily timelapses.</p>';
      }
    }

    // Event delegation for focus analysis buttons
    document.getElementById('daily-video-list').addEventListener('click', (event) => {
      if (event.target.classList.contains('analysis-btn')) {
        const dateStr = event.target.getAttribute('data-date');
        viewAnalysis(dateStr);
      }
    });

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
      const activeDayStr = getLocalDateStr(now);
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
        .reverse()
        .map(name => {
          const dateStr = name.replace('.mp4', '');
          const hasAnalysis = fs.existsSync(path.join(timelapseDir, `${dateStr}_analysis.json`));
          return { name, hasAnalysis };
        });
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
  } else if (url.pathname.match(/^\/timelapse\/\d{4}-\d{2}-\d{2}_analysis\.json$/)) {
    const filename = path.basename(url.pathname);
    const filePath = path.join(timelapseDir, filename);
    try {
      if (!fs.existsSync(filePath)) {
        res.writeHead(404, { 'Content-Type': 'text/plain' });
        res.end('Analysis file not found');
        return;
      }
      res.writeHead(200, {
        'Content-Type': 'application/json; charset=utf-8',
        'Cache-Control': 'no-cache, no-store, must-revalidate'
      });
      fs.createReadStream(filePath).pipe(res);
    } catch (err) {
      console.error('[Timelapse Analysis Stream] Error:', err);
      if (!res.headersSent) {
        res.writeHead(500, { 'Content-Type': 'text/plain' });
        res.end(err.message);
      }
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
      const currentDayStr = getLocalDateStr(now);
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
      const currentDayStr = getLocalDateStr(now);
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

    // 4. Backfill any missing focus analysis markdown files
    try {
      const files = fs.readdirSync(timelapseDir);
      for (const file of files) {
        const match = file.match(/^(\d{4}-\d{2}-\d{2})\.mp4$/);
        if (match) {
          const dateStr = match[1];
          const analysisFile = path.join(timelapseDir, `${dateStr}_analysis.json`);
          if (!fs.existsSync(analysisFile)) {
            console.log(`[Timelapse Analysis] Found missing focus analysis for ${dateStr}.mp4. Queueing backfill...`);
            analyzeVideo(dateStr).catch(err => {
              console.error(`[Timelapse Analysis] Startup analysis backfill failed for ${dateStr}:`, err);
            });
          }
        }
      }
    } catch (e) {
      console.error('[Timelapse Analysis] Startup focus analysis backfill failed:', e);
    }

    // Keep track of active hour and its frameIndex
    let activeDayStr = getLocalDateStr();
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
        const currentDayStr = getLocalDateStr(now);
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
          
          // Publish frame to Zenoh Pub/Sub Mesh
          try {
            const frameBytes = fs.readFileSync(filePath);
            await vfs.write(`jot/webcam/feed/${id}`, frameBytes);
          } catch (pubErr) {
            console.error('[Timelapse] Failed to publish frame to mesh:', pubErr);
          }

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
