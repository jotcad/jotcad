import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

// Initial Launch
console.log('[Orchestrator] Starting JotCAD System...');

try {
  console.log('[Orchestrator] Cleaning up port 3030...');
  // Use fuser to kill anything on 3030. || true to ignore errors if port is empty.
  execSync('fuser -k 3030/tcp || true', { stdio: 'inherit' });
} catch (e) {
  // Ignore
}

const components = [
  {
    name: 'VFS Hub',
    command: 'node',
    args: ['fs/serve.js'],
    cwd: __dirname,
    env: { ...process.env, PORT: '9090' }
  },
  {
    name: 'C++ Ops Service',
    command: 'node',
    args: ['geo/src/dispatcher_service.js'],
    cwd: __dirname,
    env: { ...process.env }
  },
  {
    name: 'PDF Service',
    command: 'node',
    args: ['geo/src/pdf_service.js'],
    cwd: __dirname,
    env: { ...process.env }
  },
  {
    name: 'Export Service',
    command: 'node',
    args: ['geo/src/export_service.js'],
    cwd: __dirname,
    env: { ...process.env }
  },
  {
    name: 'Interactive UX',
    command: 'npm',
    args: ['run', 'dev', '--', '--port', '3030'],
    cwd: path.join(__dirname, 'ux'),
    env: { ...process.env }
  }
];

const processes = new Map();
let shuttingDown = false;

function shutdown(reason) {
  if (shuttingDown) return;
  shuttingDown = true;
  
  if (reason) {
    console.error(`\n[Orchestrator] FATAL FAILURE: ${reason}`);
  }
  
  console.log('\n[Orchestrator] Shutting down all components...');
  for (const [name, child] of processes) {
    console.log(`[Orchestrator] Killing ${name}...`);
    child.kill();
  }
  
  // Exit with error if a failure caused the shutdown
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
    if (!shuttingDown) {
      if (code !== 0) {
        shutdown(`${component.name} exited unexpectedly with code ${code}`);
      } else {
        shutdown(`${component.name} exited prematurely`);
      }
    }
    processes.delete(component.name);
  });

  processes.set(component.name, child);
}

process.on('SIGINT', () => shutdown());
process.on('SIGTERM', () => shutdown());

// Initial Launch
console.log('[Orchestrator] Starting JotCAD System...');

const hub = components.find(c => c.name === 'VFS Hub');
const rest = components.filter(c => c.name !== 'VFS Hub');

launch(hub);

console.log('[Orchestrator] Waiting for VFS Hub to be ready...');
setTimeout(() => {
    if (!shuttingDown) {
        rest.forEach(launch);
    }
}, 1500);

// Periodic check to ensure all processes are still in the Map and active
setInterval(() => {
    if (shuttingDown) return;
    for (const [name, child] of processes) {
        if (child.exitCode !== null) {
            shutdown(`${name} died with exit code ${child.exitCode}`);
        }
    }
}, 1000);
