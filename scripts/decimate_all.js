import fs from 'fs';
import path from 'path';
import { spawn, exec } from 'child_process';
import { promisify } from 'util';

const execAsync = promisify(exec);
const timelapseDir = path.resolve('./timelapse');

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

async function decimateFile(filePath) {
  const tempPath = filePath + '.temp.mp4';
  const filtergraph = 'split=2[full][masked];[masked]drawbox=x=0:y=0:w=450:h=60:color=black:t=fill,mpdecimate=hi=64*20:lo=64*10:frac=0.4[deduped];[deduped][full]overlay=shortest=1,setpts=N/5/TB';

  console.log(`[Decimate] Processing ${path.basename(filePath)}...`);

  await new Promise((resolve, reject) => {
    const ffmpeg = spawn('ffmpeg', [
      '-y',
      '-i', filePath,
      '-vf', filtergraph,
      '-c:v', 'libx264',
      '-pix_fmt', 'yuv420p',
      '-g', '1',
      '-movflags', '+faststart',
      tempPath
    ]);
    ffmpeg.on('close', (code) => {
      if (code === 0) resolve();
      else reject(new Error(`ffmpeg exited with code ${code}`));
    });
    ffmpeg.on('error', reject);
  });

  const origFrames = await getFrameCount(filePath);
  const newFrames = await getFrameCount(tempPath);

  fs.copyFileSync(tempPath, filePath);
  fs.unlinkSync(tempPath);

  const jsonPath = filePath + '.json';
  fs.writeFileSync(jsonPath, JSON.stringify({ count: newFrames }));

  console.log(`[Decimate] Finished ${path.basename(filePath)}. Frames: ${origFrames} -> ${newFrames}.`);
}

async function run() {
  try {
    const dateDirs = fs.readdirSync(timelapseDir).filter(f => {
      const fp = path.join(timelapseDir, f);
      return fs.statSync(fp).isDirectory() && f.match(/^\d{4}-\d{2}-\d{2}$/);
    });

    for (const dateDir of dateDirs) {
      const datePath = path.join(timelapseDir, dateDir);
      const files = fs.readdirSync(datePath).filter(f => f.match(/^hour_\d{2}\.mp4$/));
      for (const file of files) {
        const filePath = path.join(datePath, file);
        await decimateFile(filePath).catch(err => {
          console.error(`[Decimate] Error decimating ${file}:`, err);
        });
      }
    }
    console.log('[Decimate] All existing segments processed.');
  } catch (err) {
    console.error('[Decimate] Error in run:', err);
  }
}

run();
