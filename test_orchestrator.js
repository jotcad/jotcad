import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Ports for Test Cluster
const PORT_OPS = process.env.TEST_OPS_PORT || '9099';
const PORT_EXPORT = process.env.TEST_EXPORT_PORT || '9098';
const PORT_UX = process.env.TEST_UX_PORT || '3033';

console.log(`[Test Orchestrator] Starting JotCAD Test Cluster...`);
console.log(`[Test Orchestrator] Ports: Ops=${PORT_OPS}, Export=${PORT_EXPORT}, UX=${PORT_UX}`);

try {
  console.log(`[Test Orchestrator] Cleaning up ports ${PORT_UX}, ${PORT_OPS}, ${PORT_EXPORT} and VFS storage...`);
  execSync(`fuser -k ${PORT_UX}/tcp ${PORT_OPS}/tcp ${PORT_EXPORT}/tcp || true`, { stdio: 'ignore' });
  // Clear VFS storage
  execSync(`rm -rf .vfs_storage_test-geo-ops-node .vfs_storage_test-export-node || true`);
  execSync('sleep 1');
} catch (e) {}

const components = [
  {
    name: 'Test Ops Node',
    command: './geo/bin/ops',
    args: [PORT_OPS, '.vfs_storage_test-geo-ops-node'],
    cwd: __dirname,
    env: { 
        ...process.env, 
        PORT: PORT_OPS,
        PEER_ID: 'test-geo-ops-node'
    }
  },
  {
    name: 'Test Export Node',
    command: 'node',
    args: ['geo/export_service.js'],
    cwd: __dirname,
    env: { 
        ...process.env, 
        PORT: PORT_EXPORT,
        VFS_ID: 'test-export-node',
        NEIGHBORS: `http://localhost:${PORT_OPS}`
    }
  },
  {
    name: 'Test UX',
    command: 'npm',
    args: ['run', 'dev', '--', '--port', PORT_UX, '--strictPort'],
    cwd: path.join(__dirname, 'ux'),
    env: {
        ...process.env,
        VITE_STORAGE_PREFIX: 'test',
        VITE_VFS_URL: `http://localhost:${PORT_EXPORT}` // Connect UX to Export node (which peers with Ops)
    }
  }
];

const processes = new Map();
let shuttingDown = false;

function shutdown(reason) {
  if (shuttingDown) return;
  shuttingDown = true;
  if (reason) console.error(`\n[Test Orchestrator] FATAL FAILURE: ${reason}`);
  console.log('\n[Test Orchestrator] Shutting down all test components...');
  for (const [name, child] of processes) {
    console.log(`[Test Orchestrator] Killing ${name}...`);
    child.kill();
  }
  process.exit(reason ? 1 : 0);
}

function launch(component) {
  console.log(`[Test Orchestrator] Launching ${component.name}...`);
  const child = spawn(component.command, component.args, {
    cwd: component.cwd,
    env: component.env || process.env,
    stdio: 'pipe'
  });

  child.stdout.on('data', (data) => {
    process.stdout.write(`[${component.name}] ${data}`);
  });

  child.stderr.on('data', (data) => {
    process.stderr.write(`[${component.name} ERROR] ${data}`);
  });

  child.on('error', (err) => {
    shutdown(`${component.name} failed to start: ${err.message}`);
  });

  child.on('close', (code) => {
    if (!shuttingDown && code !== 0) {
        shutdown(`${component.name} exited unexpectedly with code ${code}`);
    }
    processes.delete(component.name);
  });

  processes.set(component.name, child);
}

components.forEach(launch);

process.on('SIGINT', () => shutdown());
process.on('SIGTERM', () => shutdown());

setInterval(() => {
    if (shuttingDown) return;
    for (const [name, child] of processes) {
        if (child.exitCode !== null) {
            shutdown(`${name} died with exit code ${child.exitCode}`);
        }
    }
}, 1000);
