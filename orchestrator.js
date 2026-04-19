import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Initial Launch
console.log('[Orchestrator] Starting JotCAD System...');

try {
  console.log('[Orchestrator] Cleaning up ports 3030, 9091, 9092...');
  // Force kill anything on these ports before starting
  execSync('fuser -k 3030/tcp 9091/tcp 9092/tcp || true', { stdio: 'ignore' });
  // Give it a moment to release
  execSync('sleep 2');
} catch (e) {}

const components = [
  {
    name: 'Ops Node (9091)',
    command: './geo/bin/ops',
    args: [],
    cwd: __dirname,
    env: { 
        ...process.env, 
        PORT: '9091',
        PEER_ID: 'geo-ops-node',
        NEIGHBORS: '' 
        }

  },
  {
    name: 'Export Node (9092)',
    command: 'node',
    args: ['geo/export_service.js'], // We should update this to be a native VFS node too
    cwd: __dirname,
    env: { 
        ...process.env, 
        PORT: '9092',
        PEER_ID: 'export-node',
        NEIGHBORS: 'http://localhost:9091' // Export uses Ops
    }
  },
  {
    name: 'Interactive UX',
    command: 'npm',
    args: ['run', 'dev', '--', '--port', '3030'],
    cwd: path.join(__dirname, 'ux'),
    env: { 
        ...process.env,
        VITE_VFS_URL: 'http://localhost:9092'
    }
  }
];

const processes = new Map();
let shuttingDown = false;

function shutdown(reason) {
  if (shuttingDown) return;
  shuttingDown = true;
  if (reason) console.error(`\n[Orchestrator] FATAL FAILURE: ${reason}`);
  console.log('\n[Orchestrator] Shutting down all components...');
  for (const [name, child] of processes) {
    console.log(`[Orchestrator] Killing ${name}...`);
    child.kill();
  }
  process.exit(reason ? 1 : 0);
}

function launch(component) {
  console.log(`[Orchestrator] Launching ${component.name}...`);
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

// Initial Launch
console.log('[Orchestrator] Starting JotCAD Mesh-VFS...');
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
