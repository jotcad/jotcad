import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import { info, warn, error } from './fs/src/log.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function getSplitOpsComponents(routerPort, startPort) {
  return {
    ops_primitives: { type: 'ops_primitives', protocol: 'http', port: startPort,     env: { PEER_ID: 'geo-primitives-node', NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_booleans:   { type: 'ops_booleans',   protocol: 'http', port: startPort + 1, env: { PEER_ID: 'geo-booleans-node',   NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_transforms: { type: 'ops_transforms', protocol: 'http', port: startPort + 2, env: { PEER_ID: 'geo-transforms-node', NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_features:   { type: 'ops_features',   protocol: 'http', port: startPort + 3, env: { PEER_ID: 'geo-features-node',   NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_mapping:    { type: 'ops_mapping',    protocol: 'http', port: startPort + 4, env: { PEER_ID: 'geo-mapping-node',    NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_sweep:      { type: 'ops_sweep',      protocol: 'http', port: startPort + 5, env: { PEER_ID: 'geo-sweep-node',      NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_unfold:     { type: 'ops_unfold',     protocol: 'http', port: startPort + 6, env: { PEER_ID: 'geo-unfold-node',     NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_tooling:    { type: 'ops_tooling',    protocol: 'http', port: startPort + 7, env: { PEER_ID: 'geo-tooling-node',    NEIGHBORS: `http://127.0.0.1:${routerPort}` } },
    ops_io:         { type: 'ops_io',         protocol: 'http', port: startPort + 8, env: { PEER_ID: 'geo-io-node',         NEIGHBORS: `http://127.0.0.1:${routerPort}` } }
  };
}

const findOpsNode = (map) => {
  return map.ops || map.ops_primitives || map.ops_transforms || map.ops_booleans || map.ops_features || map.ops_sweep || map.ops_io || map.ops_mapping || map.ops_unfold || map.ops_tooling;
};

/**
 * Explicit Profiles
 */
export const PROFILES = {
  'live/standard': {
    storagePrefix: '.vfs_storage_live_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000 },
      ...getSplitOpsComponents(9000, 9091),
      export: { type: 'export', protocol: 'https', port: 9092, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/standard': {
    storagePrefix: '.vfs_storage_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200 },
      ...getSplitOpsComponents(9200, 9191),
      export: { type: 'export', protocol: 'https', port: 9197, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3131, dist: 'ux/dist/test' }
    }
  },
  'live/esp32': {
    storagePrefix: '.vfs_storage_esp32_live_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000 },
      ...getSplitOpsComponents(9000, 9091),
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
      ...getSplitOpsComponents(9200, 9191),
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
      ...getSplitOpsComponents(9400, 8183)
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
      ...getSplitOpsComponents(9500, 9191),
      ux:  { type: 'ux',  protocol: 'https', port: 3232, dist: 'ux/dist/test' }
    }
  },
  'live/webcam': {
    storagePrefix: '.vfs_storage_webcam_live_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000 },
      ...getSplitOpsComponents(9000, 9091),
      export: { type: 'export', protocol: 'https', port: 9102, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      webcam: { type: 'webcam', protocol: 'https', port: 8080, env: { NEIGHBORS: 'http://127.0.0.1:9000' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3030, dist: 'ux/dist/live' }
    }
  },
  'test/webcam': {
    storagePrefix: '.vfs_storage_webcam_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200 },
      ...getSplitOpsComponents(9200, 9191),
      export: { type: 'export', protocol: 'https', port: 9202, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      webcam: { type: 'webcam', protocol: 'https', port: 8180, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      ux:     { type: 'ux',     protocol: 'https', port: 3131, dist: 'ux/dist/test' }
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

  const createOpsConfig = (binaryPath, cfg, key) => {
    const useSsl = cfg.protocol === 'https';
    const storageKey = key || path.basename(binaryPath);
    return {
      name: `${path.basename(binaryPath)} (${cfg.port})`,
      command: binaryPath,
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
  };

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
    ops: (cfg, key) => createOpsConfig('./geo/bin/ops', cfg, key),
    ops_primitives: (cfg, key) => createOpsConfig('./geo/bin/ops_primitives', cfg, key),
    ops_booleans: (cfg, key) => createOpsConfig('./geo/bin/ops_booleans', cfg, key),
    ops_transforms: (cfg, key) => createOpsConfig('./geo/bin/ops_transforms', cfg, key),
    ops_features: (cfg, key) => createOpsConfig('./geo/bin/ops_features', cfg, key),
    ops_mapping: (cfg, key) => createOpsConfig('./geo/bin/ops_mapping', cfg, key),
    ops_sweep: (cfg, key) => createOpsConfig('./geo/bin/ops_sweep', cfg, key),
    ops_unfold: (cfg, key) => createOpsConfig('./geo/bin/ops_unfold', cfg, key),
    ops_tooling: (cfg, key) => createOpsConfig('./geo/bin/ops_tooling', cfg, key),
    ops_io: (cfg, key) => createOpsConfig('./geo/bin/ops_io', cfg, key),
    export: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      const opsNode = findOpsNode(componentMap);
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
      const opsNode = findOpsNode(componentMap);
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
    },
    webcam: (cfg) => {
      const useSsl = cfg.protocol === 'https';
      const fullId = `${profileKey.replace('/', '_')}_webcam`;
      return {
        name: `Webcam Node (${cfg.port})`,
        command: 'node',
        args: ['--stack-size=65536', 'vfs_webcam_node.js'],
        cwd: __dirname,
        env: {
            ...process.env,
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: fullId,
            NEIGHBORS: cfg.env?.NEIGHBORS || '',
            WEBCAM_DEVICE: cfg.env?.WEBCAM_DEVICE || '/dev/video0',
            DISABLE_SSL: useSsl ? '0' : '1',
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
