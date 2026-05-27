import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { info, warn, error } from './fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

/**
 * Explicit Profiles
 */
export const PROFILES = {
  'live/standard': {
    storagePrefix: '.vfs_storage_live_',
    gateway: 'export',
    components: {
      ops:    { type: 'ops',    protocol: 'http',  port: 9091 },
      export: { type: 'export', protocol: 'https', port: 9092 },
      ux:     { type: 'ux',     protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/standard': {
    storagePrefix: '.vfs_storage_test_',
    gateway: 'export',
    components: {
      ops:    { type: 'ops',    protocol: 'http',  port: 9191 },
      export: { type: 'export', protocol: 'https', port: 9194 },
      ux:     { type: 'ux',     protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  'live/esp32': {
    storagePrefix: '.vfs_storage_esp32_live_',
    gateway: 'export',
    components: {
      ops:        { type: 'ops',        protocol: 'http',  port: 9091 },
      export:     { type: 'export',     protocol: 'https', port: 9092 },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11223 },
      ux:         { type: 'ux',         protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/esp32': {
    storagePrefix: '.vfs_storage_esp32_test_',
    gateway: 'export',
    components: {
      ops:        { type: 'ops',        protocol: 'http',  port: 9191 },
      export:     { type: 'export',     protocol: 'https', port: 9192 },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11224 },
      ux:         { type: 'ux',         protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  'test/sovereign_js': {
    storagePrefix: '.vfs_storage_sovereign_js_',
    gateway: 'node_a',
    components: {
      node_a: { type: 'node_a', protocol: 'http', port: 8181 },
      node_b: { type: 'node_b', protocol: 'http', port: 8182 }
    }
  },
  'test/complex_topology': {
    storagePrefix: '.vfs_storage_complex_topo_',
    gateway: 'node_js',
    components: {
      cpp_node_1: { type: 'vfs_cpp', protocol: 'http', port: 9591 },
      cpp_node_2: { type: 'vfs_cpp', protocol: 'http', port: 9592 },
      node_js:    { type: 'node_a',  protocol: 'http', port: 9593 }
    },
    env: {
      NEIGHBORS: 'http://localhost:9591,http://localhost:9592'
    }
  },
  'live/direct_cpp': {
    storagePrefix: '.vfs_storage_direct_cpp_live_',
    gateway: 'ops',
    components: {
      ops: { type: 'ops', protocol: 'https', port: 9192 },
      ux:  { type: 'ux',  protocol: 'https', port: 3232, dist: 'ux/dist/test' }
    }
  }
};

export async function launchSystem(profileKey = 'live/standard', globalLogLevel = 'INFO', options = {}) {
  const config = PROFILES[profileKey];
  if (!config) {
    throw new Error(`Unknown profile: ${profileKey}. Available: ${Object.keys(PROFILES).join(', ')}`);
  }

  const capturedLogs = [];

  const { storagePrefix, components: componentMap, gateway, env = {} } = config;

  const sslDir = path.join(__dirname, '.ssl');
  const keyPath = path.join(sslDir, 'localhost-key.pem');
  const certPath = path.join(sslDir, 'localhost-cert.pem');
  const hasCerts = fs.existsSync(keyPath) && fs.existsSync(certPath);

  info(`[Orchestrator] Launching ${profileKey.toUpperCase()} Cluster (Log: ${globalLogLevel})...`);

  const ports = Object.fromEntries(Object.entries(componentMap).map(([id, cfg]) => [id, cfg.port]));

  try {
    const portList = Object.values(ports).map(p => `${p}/tcp`).join(' ');
    info(`[Orchestrator] Cleaning up ports: ${portList}`);
    execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
    execSync(`rm -rf ${storagePrefix}* || true`);
    execSync('sleep 1');
  } catch (e) {}

  const gatewayNode = componentMap[gateway];
  const gatewayUrl = `${gatewayNode.protocol}://localhost:${gatewayNode.port}`;

  const componentConfigs = {
    ops: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      return {
        name: `Ops Node (${cfg.port})`,
        command: './geo/bin/ops',
        args: [String(cfg.port), `${storagePrefix}ops`],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            PEER_ID: `${profileKey.replace('/', '_')}_ops`,
            SSL_CERT_PATH: (useSsl && hasCerts) ? certPath : '',
            SSL_KEY_PATH: (useSsl && hasCerts) ? keyPath : '',
            ...env
        }
      };
    },
    export: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      return {
        name: `Export Node (${cfg.port})`,
        command: 'node',
        args: ['geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: `${profileKey.replace('/', '_')}_export`,
            NEIGHBORS: `${componentMap.ops?.protocol || 'http'}://localhost:${componentMap.ops?.port || 80}`,
            DISABLE_SSL: useSsl ? '0' : '1',
            ...env
        }
      };
    },
    node_a: (cfg) => ({
      name: `JS Node A (${cfg.port})`,
      command: 'node',
      args: ['geo/export_service.js'],
      cwd: __dirname,
      env: { 
          ...process.env, 
          LOG_LEVEL: globalLogLevel,
          PORT: String(cfg.port),
          VFS_ID: `node_a`,
          DISABLE_SSL: '1',
          ...env
      }
    }),
    node_b: (cfg) => ({
      name: `JS Node B (${cfg.port})`,
      command: 'node',
      args: ['geo/export_service.js'],
      cwd: __dirname,
      env: { 
          ...process.env, 
          LOG_LEVEL: globalLogLevel,
          PORT: String(cfg.port),
          VFS_ID: `node_b`,
          NEIGHBORS: `http://localhost:${componentMap.node_a.port}`,
          DISABLE_SSL: '1',
          ...env
      }
    }),
    subscriber: (cfg) => ({
      name: `Counter Subscriber (${cfg.port})`,
      command: 'node',
      args: ['pio/counter_subscriber_node.js'],
      cwd: __dirname,
      env: { 
          ...process.env, 
          LOG_LEVEL: globalLogLevel,
          PORT: String(cfg.port),
          VFS_ID: `${profileKey.replace('/', '_')}_subscriber`,
          NEIGHBORS: `${componentMap.ops?.protocol || 'http'}://localhost:${componentMap.ops?.port || 80}`,
          ...env
      }
    }),
    vfs_cpp: (cfg, key) => {
      const fullId = `${profileKey.replace('/', '_')}_${key}`;
      return {
        name: `${key} (${cfg.port})`,
        command: './fs/cpp/test_server',
        args: ['--port', String(cfg.port), '--storage', `${storagePrefix}${key}`, '--id', fullId],
        cwd: __dirname,
        env: {
            ...process.env,
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            PEER_ID: fullId,
            ...env
        }
      };
    },
    ux: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      const uxArgs = ['http-server', cfg.dist, '-p', String(cfg.port)];
      if (useSsl && hasCerts) uxArgs.push('--ssl', '--key', '.ssl/localhost-key.pem', '--cert', '.ssl/localhost-cert.pem');

      return {
        name: `UX (${cfg.port})`,
        command: 'npx',
        args: uxArgs,
        cwd: __dirname,
        env: {
            ...process.env,
            LOG_LEVEL: globalLogLevel,
            VITE_VFS_URL: gatewayUrl,
            VITE_HTTPS: String(useSsl && hasCerts),
            ...env
        }
      };
    }
  };

  const components = Object.entries(componentMap).map(([id, cfg]) => componentConfigs[cfg.type](cfg, id));

  const processes = new Map();
  let shuttingDown = false;

  const shutdown = async () => {
    if (shuttingDown) return;
    shuttingDown = true;
    info(`\n[Orchestrator] Shutting down ${profileKey.toUpperCase()} cluster...`);
    for (const [name, child] of processes) {
      info(`[Orchestrator] Killing ${name}...`);
      child.kill('SIGTERM');
    }
    await new Promise(r => setTimeout(r, 1000));
    try {
        const portList = Object.values(ports).map(p => `${p}/tcp`).join(' ');
        execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
    } catch (e) {}
  };

  const launch = (component) => {
    info(`[Orchestrator] Starting ${component.name}...`);
    const child = spawn(component.command, component.args, {
      cwd: component.cwd,
      env: component.env,
      stdio: 'pipe'
    });

    // TRANSPARENT RELAY: All output is printed with a prefix, regardless of content.
    // The individual nodes' internal LOG_LEVEL decides what hits stdout/stderr.
    const relay = (stream, colorPrefix = '') => {
        stream.on('data', (data) => {
            const lines = data.toString().split('\n');
            for (const line of lines) {
                if (line.trim()) {
                    const formattedLine = `[${profileKey}:${component.name}] ${line.trim()}`;
                    process.stdout.write(`${colorPrefix}${formattedLine}\n`);
                    if (options.captureLogs) {
                        capturedLogs.push(formattedLine);
                    }
                }
            }
        });
    };

    relay(child.stdout);
    relay(child.stderr, '\x1b[31m'); // Red for stderr

    child.on('close', (code) => {
      if (!shuttingDown && code !== 0 && code !== null) {
          error(`[Orchestrator] ${component.name} exited unexpectedly with code ${code}`);
      }
      processes.delete(component.name);
    });

    processes.set(component.name, child);
  };

  components.forEach(launch);

  if (ports.ux) {
      info(`[Orchestrator] Waiting for UX on port ${ports.ux}...`);
      let uxReady = false;
      for (let i = 0; i < 50; i++) {
        try {
          const url = `${componentMap.ux.protocol}://localhost:${ports.ux}`;
          const options = {};
          if (componentMap.ux.protocol === 'https') options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
          const resp = await fetch(url, options);
          if (resp.ok) { uxReady = true; break; }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 200));
      }
      if (!uxReady) warn(`[Orchestrator] UX port ${ports.ux} not responding after 10s.`);
  }

  return { processes, ports, stop: shutdown, logs: capturedLogs };
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
    const args = process.argv.slice(2);
    const isTest = args.includes('--test');
    
    // Explicit Log Level Control
    let logLevel = process.env.LOG_LEVEL || 'INFO';
    const logIdx = args.indexOf('--log');
    if (logIdx !== -1 && args[logIdx+1]) {
        logLevel = args[logIdx+1].toUpperCase();
    } else if (args.includes('--debug')) {
        logLevel = 'DEBUG';
    }

    const profileName = args.find(arg => !arg.startsWith('--')) || 'standard';
    const profileKey = `${isTest ? 'test' : 'live'}/${profileName}`;
    
    try {
        const sys = await launchSystem(profileKey, logLevel);
        process.on('SIGINT', () => sys.stop().then(() => process.exit(0)));
        process.on('SIGTERM', () => sys.stop().then(() => process.exit(0)));
    } catch (err) {
        error(`[Orchestrator] Failed to launch: ${err.message}`);
        process.exit(1);
    }
}
