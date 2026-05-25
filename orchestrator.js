import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { info, warn, error, debug } from './fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

export const PROFILES = {
  LIVE: {
    name: 'LIVE',
    storagePrefix: '.vfs_storage_live_',
    gateway: 'export',
    components: {
      ops:    { protocol: 'http', port: 9091 },
      export: { protocol: 'http', port: 9092 },
      ux:     { protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  TEST: {
    name: 'TEST',
    storagePrefix: '.vfs_storage_test_',
    gateway: 'export',
    components: {
      ops:    { protocol: 'http', port: 9191 },
      export: { protocol: 'http', port: 9192 },
      ux:     { protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  DIRECT_CPP: {
    name: 'DIRECT_CPP',
    storagePrefix: '.vfs_storage_direct_cpp_',
    gateway: 'ops',
    components: {
      ops:    { protocol: 'https', port: 9192 },
      ux:     { protocol: 'https', port: 3232, dist: 'ux/dist/test' }
    }
  }
};

export async function launchSystem(profileOrConfig = PROFILES.LIVE) {
  const config = typeof profileOrConfig === 'string' ? PROFILES[profileOrConfig] : profileOrConfig;
  const { storagePrefix, components: componentMap, gateway, env = {} } = config;

  const sslDir = path.join(__dirname, '.ssl');
  const keyPath = path.join(sslDir, 'localhost-key.pem');
  const certPath = path.join(sslDir, 'localhost-cert.pem');
  const hasCerts = fs.existsSync(keyPath) && fs.existsSync(certPath);

  info(`[Orchestrator] Launching ${config.name} Cluster...`);

  const ports = Object.fromEntries(Object.entries(componentMap).map(([id, cfg]) => [id, cfg.port]));

  try {
    const portList = Object.values(ports).map(p => `${p}/tcp`).join(' ');
    info(`[Orchestrator] Cleaning up ports: ${portList}`);
    execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
    // Clear storage for this specific profile
    execSync(`rm -rf ${storagePrefix}* || true`);
    execSync('sleep 1');
  } catch (e) {}

  const gatewayNode = componentMap[gateway];
  const gatewayUrl = `${gatewayNode.protocol}://localhost:${gatewayNode.port}`;

  const componentConfigs = {
    ops: () => {
      const cfg = componentMap.ops;
      const useSsl = cfg.protocol === 'https';
      if (useSsl && !hasCerts) {
          warn(`[Orchestrator] Ops Node requested HTTPS but certs are missing. Falling back to HTTP.`);
      }
      return {
        name: `Ops Node (${cfg.port})`,
        command: './geo/bin/ops',
        args: [String(cfg.port), `${storagePrefix}ops`],
        cwd: __dirname,
        env: { 
            ...process.env, 
            PORT: String(cfg.port),
            PEER_ID: 'geo-ops-node',
            NEIGHBORS: '',
            SSL_CERT_PATH: (useSsl && hasCerts) ? certPath : '',
            SSL_KEY_PATH: (useSsl && hasCerts) ? keyPath : '',
            ...env
        }
      };
    },
    export: () => {
      const cfg = componentMap.export;
      return {
        name: `Export Node (${cfg.port})`,
        command: 'node',
        args: ['geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            PORT: String(cfg.port),
            VFS_ID: config.name === 'LIVE' ? 'live_export' : 'test_export',
            NEIGHBORS: `${componentMap.ops.protocol}://localhost:${componentMap.ops.port}`,
            ...env
        }
      };
    },
    ux: () => {
      const cfg = componentMap.ux;
      const useSsl = cfg.protocol === 'https';
      const uxArgs = ['http-server', cfg.dist, '-p', String(cfg.port)];
      
      if (useSsl && hasCerts) {
          uxArgs.push('--ssl', '--key', '.ssl/localhost-key.pem', '--cert', '.ssl/localhost-cert.pem');
      } else if (useSsl) {
          warn(`[Orchestrator] UX requested HTTPS but certs are missing. Falling back to HTTP.`);
      }

      return {
        name: `UX (${cfg.port})`,
        command: 'npx',
        args: uxArgs,
        cwd: cfg.cwd || __dirname,
        env: {
            ...process.env,
            VITE_VFS_URL: gatewayUrl,
            VITE_HTTPS: String(useSsl && hasCerts),
            ...env
        }
      };
    }
  };

  const components = Object.keys(componentMap).map(id => componentConfigs[id]());


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

  const uxCfg = componentMap.ux;
  const uxProtocol = (uxCfg && uxCfg.protocol === 'https' && hasCerts) ? 'https' : 'http';

  return { 
      processes, 
      ports,
      isHttps: uxProtocol === 'https',
      uxProtocol,
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
