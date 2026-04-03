import { spawn } from 'node:child_process';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

const components = [
  {
    name: 'VFS Hub',
    command: 'node',
    args: ['fs/serve.js'],
    cwd: __dirname,
    env: { ...process.env, PORT: '9090' }
  },
  {
    name: 'C++ Dispatcher',
    command: 'node',
    args: [
      '-e',
      `
import { VFS } from './fs/src/index.js';
import { Dispatcher } from './geo/src/dispatcher.js';
const vfs = new VFS({ id: 'dispatcher' });
const d = new Dispatcher(vfs, { 
    hubUrl: 'http://localhost:9090/vfs', 
    binDir: './geo/bin' 
});
d.register('shape/box', 'box_agent');
d.register('shape/triangle', 'triangle_agent');
d.start().catch(err => {
    console.error('[Dispatcher Error]', err);
    process.exit(1);
});
      `
    ],
    cwd: __dirname
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
components.forEach(launch);

// Periodic check to ensure all processes are still in the Map and active
setInterval(() => {
    if (shuttingDown) return;
    for (const [name, child] of processes) {
        if (child.exitCode !== null) {
            shutdown(`${name} died with exit code ${child.exitCode}`);
        }
    }
}, 1000);
