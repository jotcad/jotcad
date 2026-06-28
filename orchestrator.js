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
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000 },
      ops:    { type: 'ops',    protocol: 'http',  port: 9091, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      export: { type: 'export', protocol: 'https', port: 9092, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/standard': {
    storagePrefix: '.vfs_storage_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200 },
      ops:    { type: 'ops',    protocol: 'http',  port: 9191, env: { PEER_ID: 'geo-ops-node', NEIGHBORS: 'http://127.0.0.1:9200' } },
      ops2:   { type: 'ops',    protocol: 'http',  port: 9192, env: { PEER_ID: 'geo-ops-node-2', NEIGHBORS: 'http://127.0.0.1:9200' } },
      export: { type: 'export', protocol: 'https', port: 9197, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  'live/esp32': {
    storagePrefix: '.vfs_storage_esp32_live_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000 },
      ops:        { type: 'ops',        protocol: 'http',  port: 9091, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      export:     { type: 'export',     protocol: 'https', port: 9092, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11223, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      ux:         { type: 'ux',         protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/esp32': {
    storagePrefix: '.vfs_storage_esp32_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200 },
      ops:        { type: 'ops',        protocol: 'http',  port: 9191, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      export:     { type: 'export',     protocol: 'https', port: 9192, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11224, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      ux:         { type: 'ux',         protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  'test/sovereign_js': {
    storagePrefix: '.vfs_storage_sovereign_js_',
    gateway: 'node_a',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9400 },
      node_a: { type: 'node_a', protocol: 'http', port: 8181, env: { NEIGHBORS: 'http://127.0.0.1:9400' } },
      node_b: { type: 'node_b', protocol: 'http', port: 8182, env: { NEIGHBORS: 'http://127.0.0.1:9400' } },
      ops:    { type: 'ops',    protocol: 'http', port: 8183, env: { NEIGHBORS: 'http://127.0.0.1:9400' } }
    }
  },
  'test/complex_topology': {
    storagePrefix: '.vfs_storage_complex_topo_',
    gateway: 'node_js',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9300 },
      cpp_node_1: { type: 'vfs_cpp', protocol: 'http', port: 9591, env: { NEIGHBORS: 'http://127.0.0.1:9300' } },
      cpp_node_2: { type: 'vfs_cpp', protocol: 'http', port: 9592, env: { NEIGHBORS: 'http://127.0.0.1:9300' } },
      node_js:    { type: 'node_a',  protocol: 'http', port: 9593, env: { NEIGHBORS: 'http://127.0.0.1:9300' } }
    }
  },
  'live/direct_cpp': {
    storagePrefix: '.vfs_storage_direct_cpp_live_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9500 },
      ops: { type: 'ops', protocol: 'https', port: 9198, env: { PEER_ID: 'geo-ops-node', NEIGHBORS: 'http://127.0.0.1:9500' } },
      ux:  { type: 'ux',  protocol: 'https', port: 3232, dist: 'ux/dist/test' }
    }
  }
};

export async function launchSystem(profileKey, globalLogLevel = process.env.LOG_LEVEL || 'INFO', options = {}) {
  process.env.ZENOH_RUNTIME = '( app: (worker_threads: 8) )';
  if (!profileKey) {
    throw new Error('CRITICAL: No profileKey provided to launchSystem. You must explicitly choose a profile (e.g., "test/standard").');
  }
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
  if (!gatewayNode) {
      throw new Error(`Profile "${profileKey}" specifies gateway "${gateway}", but that component is not defined.`);
  }

  const gatewayPort = gatewayNode.port;
  if (!gatewayPort || gatewayPort <= 0 || gatewayPort > 65535) {
      throw new Error(`Invalid gateway port for "${gateway}": ${gatewayPort}. Port must be between 1 and 65535.`);
  }

  let gatewayUrl = '';
  if (componentMap.ux) {
      gatewayUrl = `${componentMap.ux.protocol}://localhost:${componentMap.ux.port}?gateway=${gatewayPort}`;
  }

  const componentConfigs = {
    zenoh_router: (cfg) => {
      return [
        {
          name: `Zenoh Router (${cfg.port})`,
          command: '/home/brian/.cargo/bin/zenohd',
          args: ['-l', `tcp/0.0.0.0:${cfg.port}`, '--no-multicast-scouting'],
          cwd: __dirname,
          env: { ...process.env }
        },
        {
          name: `Zenoh Bridge (${cfg.port})`,
          command: '/home/brian/.cargo/bin/zenoh-bridge-remote-api',
          args: ['-e', `tcp/127.0.0.1:${cfg.port}`, '--ws-port', String(cfg.port + 1000), '--no-multicast-scouting'],
          cwd: __dirname,
          env: { ...process.env }
        }
      ];
    },
    ops: (cfg, key) => {
      const useSsl = cfg.protocol === 'https';
      const storageKey = key || 'ops';
      return {
        name: `Ops Node (${cfg.port})`,
        command: './geo/bin/ops',
        args: [String(cfg.port), `${storagePrefix}${storageKey}`],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            PEER_ID: `${profileKey.replace('/', '_')}_${storageKey}`,
            SSL_CERT_PATH: (useSsl && hasCerts) ? certPath : '',
            SSL_KEY_PATH: (useSsl && hasCerts) ? keyPath : '',
            ...env,
            ...cfg.env
        }
      };
    },
    export: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      const opsNode = componentMap.ops || componentMap.ops1 || componentMap.ops2;
      return {
        name: `Export Node (${cfg.port})`,
        command: 'node',
        args: ['--stack-size=65536', 'geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: `${profileKey.replace('/', '_')}_export`,
            NEIGHBORS: `${opsNode?.protocol || 'http'}://localhost:${opsNode?.port || 80}`,
            DISABLE_SSL: useSsl ? '0' : '1',
            ...env,
            ...cfg.env
        }
      };
    },
    node_a: (cfg) => {
      const fullId = `${profileKey.replace('/', '_')}_node_a`;
      return {
        name: `JS Node A (${cfg.port})`,
        command: 'node',
        args: ['--stack-size=65536', 'geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: fullId,
            DISABLE_SSL: '1',
            ...env,
            ...cfg.env
        }
      };
    },
    node_b: (cfg) => {
      const fullId = `${profileKey.replace('/', '_')}_node_b`;
      return {
        name: `JS Node B (${cfg.port})`,
        command: 'node',
        args: ['--stack-size=65536', 'geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: fullId,
            NEIGHBORS: `http://localhost:${componentMap.node_a.port}`,
            DISABLE_SSL: '1',
            ...env,
            ...cfg.env
        }
      };
    },
    subscriber: (cfg) => {
      const opsNode = componentMap.ops || componentMap.ops1 || componentMap.ops2;
      return {
        name: `Counter Subscriber (${cfg.port})`,
        command: 'node',
        args: ['pio/counter_subscriber_node.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: `${profileKey.replace('/', '_')}_subscriber`,
            NEIGHBORS: `${opsNode?.protocol || 'http'}://localhost:${opsNode?.port || 80}`,
            ...env,
            ...cfg.env
        }
      };
    },
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
            ...env,
            ...cfg.env
        }
      };
    },
    ux: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      const uxArgs = ['http-server', cfg.dist, '-p', String(cfg.port), '-c-1'];
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
            ...env,
            ...cfg.env
        }
      };
    }
  };

  const components = Object.entries(componentMap).flatMap(([id, cfg]) => {
    const res = componentConfigs[cfg.type](cfg, id);
    return Array.isArray(res) ? res : [res];
  });

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
    for (const [name, child] of processes) {
      try {
        child.kill('SIGKILL');
      } catch (e) {}
    }
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
      const protocol = (componentMap.ux.protocol === 'https' && hasCerts) ? 'https' : 'http';
      for (let i = 0; i < 50; i++) {
        try {
          const url = `${protocol}://localhost:${ports.ux}`;
          const options = {};
          if (protocol === 'https') options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
          const resp = await fetch(url, options);
          if (resp.ok || resp.status === 404) { uxReady = true; break; }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 200));
      }
      if (!uxReady) warn(`[Orchestrator] UX port ${ports.ux} not responding after 10s.`);
  }

  return { 
    processes, 
    ports, 
    gatewayPort,
    gatewayUrl,
    stop: shutdown, 
    logs: capturedLogs 
  };
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
