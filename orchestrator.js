import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { info, warn, error, debug } from './fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export const PROFILES = {
  LIVE: {
    name: 'LIVE',
    ports: { ops: 9091, export: 9092, ux: 3030 },
    storagePrefix: '.vfs_storage_live_',
    ux: {
        dist: 'ux/dist/live'
    }
  },
  TEST: {
    name: 'TEST',
    ports: { ops: 9191, export: 9192, ux: 3131 },
    storagePrefix: '.vfs_storage_test_',
    ux: {
        dist: 'ux/dist/test'
    }
  }
};

export async function launchSystem(profileOrConfig = PROFILES.LIVE) {
  const config = typeof profileOrConfig === 'string' ? PROFILES[profileOrConfig] : profileOrConfig;
  const { ports, storagePrefix, ux, env = {} } = config;

  const sslDir = path.join(__dirname, '.ssl');
  const keyPath = path.join(sslDir, 'localhost-key.pem');
  const certPath = path.join(sslDir, 'localhost-cert.pem');
  const hasCerts = fs.existsSync(keyPath) && fs.existsSync(certPath);

  info(`[Orchestrator] Launching ${config.name} Cluster...`);

  try {
    const portList = Object.values(ports).map(p => `${p}/tcp`).join(' ');
    info(`[Orchestrator] Cleaning up ports: ${portList}`);
    execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
    // Clear storage for this specific profile
    execSync(`rm -rf ${storagePrefix}* || true`);
    execSync('sleep 1');
  } catch (e) {}

  const uxArgs = ['http-server', ux.dist, '-p', String(ports.ux)];
  if (hasCerts) {
      uxArgs.push('--ssl', '--key', '.ssl/localhost-key.pem', '--cert', '.ssl/localhost-cert.pem');
  } else {
      console.warn(`[Orchestrator] SSL certificates not found in .ssl/. Falling back to HTTP for UX.`);
  }

  const components = [
    {
      name: `Ops Node (${ports.ops})`,
      command: './geo/bin/ops',
      args: [String(ports.ops), `${storagePrefix}ops`],
      cwd: __dirname,
      env: { 
          ...process.env, 
          PORT: String(ports.ops),
          PEER_ID: 'geo-ops-node',
          NEIGHBORS: '',
          ...env
      }
    },
    {
        name: `Export Node (${ports.export})`,
        command: 'node',
        args: ['geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            PORT: String(ports.export),
            VFS_ID: config.name === 'LIVE' ? 'live_export' : 'test_export',
            NEIGHBORS: `http://localhost:${ports.ops}`,
            ...env
        }
    },
    {
      name: `UX (${ports.ux})`,
      command: 'npx',
      args: uxArgs,
      cwd: ux.cwd || __dirname,
      env: {
          ...process.env,
          VITE_VFS_URL: `${hasCerts ? 'https' : 'http'}://localhost:${ports.export}`,
          VITE_HTTPS: String(hasCerts),
          ...env
      }
    }
  ];

  const processes = new Map();
  let shuttingDown = false;

  const shutdown = async () => {
    if (shuttingDown) return;
    shuttingDown = true;
    info(`\n[Orchestrator] Shutting down ${config.name} cluster...`);
    for (const [name, child] of processes) {
      info(`[Orchestrator] Killing ${name}...`);
      child.kill('SIGTERM');
    }
    // Give child processes time to gracefully exit and release ports
    await new Promise(r => setTimeout(r, 1000));
    
    // Force cleanup ports
    try {
        const portList = Object.values(ports).map(p => `${p}/tcp`).join(' ');
        execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
        await new Promise(r => setTimeout(r, 500));
    } catch (e) {}
  };

  const launch = (component) => {
    info(`[Orchestrator] Starting ${component.name}...`);
    const child = spawn(component.command, component.args, {
      cwd: component.cwd,
      env: component.env || process.env,
      stdio: 'pipe'
    });

    child.stdout.on('data', (data) => debug(`[${config.name}:${component.name}] ${data.toString().trim()}`));
    child.stderr.on('data', (data) => warn(`[${config.name}:${component.name} ERROR] ${data.toString().trim()}`));

    child.on('error', (err) => {
      error(`[Orchestrator] ${component.name} failed to start: ${err.message}`);
    });

    child.on('close', (code) => {
      if (!shuttingDown && code !== 0 && code !== null) {
          error(`[Orchestrator] ${component.name} exited unexpectedly with code ${code}`);
      }
      processes.delete(component.name);
    });

    processes.set(component.name, child);
  };

  components.forEach(launch);

  // Wait for UX to be healthy
  info(`[Orchestrator] Waiting for UX on port ${ports.ux}...`);
  let uxReady = false;
  for (let i = 0; i < 50; i++) {
    try {
      const url = `${hasCerts ? 'https' : 'http'}://localhost:${ports.ux}`;
      const options = {};
      
      if (hasCerts) {
          // Disable SSL verification for the health check probe since we use self-signed certs
          options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
      }

      const resp = await fetch(url, options);
      if (resp.ok) { uxReady = true; break; }
    } catch (e) {}
    await new Promise(r => setTimeout(r, 200));
  }
  if (!uxReady) warn(`[Orchestrator] UX port ${ports.ux} not responding after 10s.`);

  return { 
      processes, 
      ports,
      isHttps: hasCerts,
      stop: shutdown 
  };
}

// CLI Mode
if (process.argv[1] === fileURLToPath(import.meta.url)) {
    const isTest = process.argv.includes('--test');
    const sys = await launchSystem(isTest ? PROFILES.TEST : PROFILES.LIVE);
    process.on('SIGINT', () => sys.stop().then(() => process.exit(0)));
    process.on('SIGTERM', () => sys.stop().then(() => process.exit(0)));
}
