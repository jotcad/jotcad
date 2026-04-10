import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const binDir = path.join(__dirname, '..', 'bin');
const opsPath = path.join(binDir, 'ops');

console.log('[Ops Service] Starting persistent C++ service...');

const child = spawn(opsPath, [
  '--hub', 'http://127.0.0.1:9090/vfs',
  '--peer', 'geo-ops-service'
], {
  cwd: path.join(__dirname, '../..')
});

child.stdout.on('data', (data) => {
  process.stdout.write(`[Ops Service] ${data}`);
});

child.stderr.on('data', (data) => {
  process.stderr.write(`[Ops Service ERROR] ${data}`);
});

child.on('close', (code) => {
  console.log(`[Ops Service] Process exited with code ${code}`);
  process.exit(code);
});

// Forward termination signals
process.on('SIGINT', () => child.kill('SIGINT'));
process.on('SIGTERM', () => child.kill('SIGTERM'));
