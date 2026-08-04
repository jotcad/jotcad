import { spawn, execSync } from 'node:child_process';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
import os from 'node:os';
import net from 'node:net';
import { info, warn, error } from './fs/src/log.js';

function isPortInUse(port) {
  return new Promise((resolve) => {
    const server = net.createServer();
    server.once('error', (err) => {
      if (err.code === 'EADDRINUSE') resolve(true);
      else resolve(false);
    });
    server.once('listening', () => {
      server.close();
      resolve(false);
    });
    server.listen(port, '127.0.0.1');
  });
}

const __dirname = path.dirname(fileURLToPath(import.meta.url));

function getSplitOpsComponents(routerPort, startPort, storagePrefix, peerPrefix) {
  return {
    ops_primitives: { type: 'ops_primitives', port: startPort,     storage: `${storagePrefix}ops_primitives`, peer_id: `${peerPrefix}_ops_primitives`, neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_booleans:   { type: 'ops_booleans',   port: startPort + 1, storage: `${storagePrefix}ops_booleans`,   peer_id: `${peerPrefix}_ops_booleans`,   neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_transforms: { type: 'ops_transforms', port: startPort + 2, storage: `${storagePrefix}ops_transforms`, peer_id: `${peerPrefix}_ops_transforms`, neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_features:   { type: 'ops_features',   port: startPort + 3, storage: `${storagePrefix}ops_features`,   peer_id: `${peerPrefix}_ops_features`,   neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_mapping:    { type: 'ops_mapping',    port: startPort + 4, storage: `${storagePrefix}ops_mapping`,    peer_id: `${peerPrefix}_ops_mapping`,    neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_sweep:      { type: 'ops_sweep',      port: startPort + 5, storage: `${storagePrefix}ops_sweep`,      peer_id: `${peerPrefix}_ops_sweep`,      neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_unfold:     { type: 'ops_unfold',     port: startPort + 6, storage: `${storagePrefix}ops_unfold`,     peer_id: `${peerPrefix}_ops_unfold`,     neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_tooling:    { type: 'ops_tooling',    port: startPort + 7, storage: `${storagePrefix}ops_tooling`,    peer_id: `${peerPrefix}_ops_tooling`,    neighbors: [`tcp/127.0.0.1:${routerPort}`] },
    ops_io:         { type: 'ops_io',         port: startPort + 8, storage: `${storagePrefix}ops_io`,         peer_id: `${peerPrefix}_ops_io`,         neighbors: [`tcp/127.0.0.1:${routerPort}`] }
  };
}

const findOpsNode = (map) => {
  return map.ops || map.ops_primitives || map.ops_transforms || map.ops_booleans || map.ops_features || map.ops_sweep || map.ops_io || map.ops_mapping || map.ops_unfold || map.ops_tooling;
};

/**
 * Explicit Profiles
 */
export const PROFILES = {
  'dev/standard': {
    storagePrefix: '.vfs_storage/dev_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9100, websocket_port: 10100, multicast: false },
      ...getSplitOpsComponents(9100, 14500, '.vfs_storage/dev_', 'dev_standard'),
      export: { type: 'export', protocol: 'https', port: 9102, vfs_id: 'dev_standard_export', neighbors: ['http://127.0.0.1:9100'] },
      ux:     { type: 'ux',     protocol: 'https', port: 3030, dist: 'ux/dist/dev', gateway_port: 9100 }
    }
  },
  'test/standard': {
    storagePrefix: '.vfs_storage/test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200, websocket_port: 10200, multicast: false },
      ...getSplitOpsComponents(9200, 17430, '.vfs_storage/test_', 'test_standard'),
      export: { type: 'export', protocol: 'https', port: 9202, vfs_id: 'test_standard_export', neighbors: ['http://127.0.0.1:9200'] },
      ux:     { type: 'ux',     protocol: 'https', port: 3131, dist: 'ux/dist/test', gateway_port: 9200 }
    }
  },
  'dev/esp32': {
    storagePrefix: '.vfs_storage/esp32_dev_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9100, websocket_port: 10100, multicast: false },
      ...getSplitOpsComponents(9100, 14500, '.vfs_storage/esp32_dev_', 'dev_esp32'),
      export:     { type: 'export',     protocol: 'https', port: 9102, vfs_id: 'dev_esp32_export', neighbors: ['http://127.0.0.1:9100'] },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11225, env: { NEIGHBORS: 'http://127.0.0.1:9100' } },
      ux:         { type: 'ux',         protocol: 'https', port: 3030, dist: 'ux/dist/dev', gateway_port: 9100 }
    }
  },
  'test/esp32': {
    storagePrefix: '.vfs_storage/esp32_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200, websocket_port: 10200, multicast: false },
      ...getSplitOpsComponents(9200, 17430, '.vfs_storage/esp32_test_', 'test_esp32'),
      export:     { type: 'export',     protocol: 'https', port: 9202, vfs_id: 'test_esp32_export', neighbors: ['http://127.0.0.1:9200'] },
      subscriber: { type: 'subscriber', protocol: 'http',  port: 11226, env: { NEIGHBORS: 'http://127.0.0.1:9200' } },
      ux:         { type: 'ux',         protocol: 'https', port: 3131, dist: 'ux/dist/test', gateway_port: 9200 }
    }
  },
  'test/sovereign_js': {
    storagePrefix: '.vfs_storage/sovereign_js_',
    gateway: 'node_a',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9400, websocket_port: 10400, multicast: false },
      node_a: { type: 'node_a', protocol: 'http', port: 8181, env: { NEIGHBORS: 'http://127.0.0.1:9400' } },
      node_b: { type: 'node_b', protocol: 'http', port: 8182, env: { NEIGHBORS: 'http://127.0.0.1:9400' } },
      ...getSplitOpsComponents(9400, 18183, '.vfs_storage/sovereign_js_', 'test_sovereign_js')
    }
  },
  'test/complex_topology': {
    storagePrefix: '.vfs_storage/complex_topo_',
    gateway: 'node_js',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9300, websocket_port: 10300, multicast: false },
      cpp_node_1: { type: 'vfs_cpp', protocol: 'http', port: 19591, storage: '.vfs_storage/complex_topo_cpp_node_1', peer_id: 'test_complex_topology_cpp_node_1', neighbors: ['tcp/127.0.0.1:9300'] },
      cpp_node_2: { type: 'vfs_cpp', protocol: 'http', port: 19592, storage: '.vfs_storage/complex_topo_cpp_node_2', peer_id: 'test_complex_topology_cpp_node_2', neighbors: ['tcp/127.0.0.1:9300'] },
      node_js:    { type: 'node_a',  protocol: 'http', port: 19593, env: { NEIGHBORS: 'http://127.0.0.1:9300' } }
    }
  },
  'dev/direct_cpp': {
    storagePrefix: '.vfs_storage/direct_cpp_dev_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9500, websocket_port: 10500, multicast: false },
      ...getSplitOpsComponents(9500, 18500, '.vfs_storage/direct_cpp_dev_', 'dev_direct_cpp'),
      ux:  { type: 'ux',  protocol: 'https', port: 3232, dist: 'ux/dist/test', gateway_port: 9500 }
    }
  },
  'dev/webcam': {
    storagePrefix: '.vfs_storage/webcam_dev_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9100, websocket_port: 10100, multicast: false },
      webcam: { type: 'webcam', protocol: 'https', port: 8081, env: { NEIGHBORS: 'http://127.0.0.1:9100', TIMELAPSE: 'true' } }
    }
  },
  'test/webcam': {
    storagePrefix: '.vfs_storage/webcam_test_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9200, websocket_port: 10200, multicast: false },
      webcam: { type: 'webcam', protocol: 'https', port: 8181, env: { NEIGHBORS: 'http://127.0.0.1:9200', TIMELAPSE: 'true' } }
    }
  },
  'prod': {
    storagePrefix: '.vfs_storage/prod_',
    gateway: 'zenoh_router',
    components: {
      zenoh_router: { type: 'zenoh_router', port: 9000, websocket_port: 10000, multicast: false },
      ...getSplitOpsComponents(9000, 9091, '.vfs_storage/prod_', 'prod'),
      export: { type: 'export', protocol: 'https', port: 9092, vfs_id: 'prod_export', neighbors: ['http://127.0.0.1:9000'] },
      ux:     { type: 'ux',     protocol: 'https', port: 3031, dist: 'ux/dist/prod', gateway_port: 9000 }
    }
  }
};


export async function launchSystem(profileKey, globalLogLevel = process.env.LOG_LEVEL || 'INFO', options = {}) {
  process.env.ZENOH_RUNTIME = '( app: (worker_threads: 8) )';
  if (!profileKey) {
    throw new Error('CRITICAL: No profileKey provided to launchSystem. You must explicitly choose a profile (e.g., "test/standard").');
  }
  let config = PROFILES[profileKey];
  if (!config) {
    throw new Error(`Unknown profile: ${profileKey}. Available: ${Object.keys(PROFILES).join(', ')}`);
  }

  if (options.basePort && profileKey === 'test/standard') {
    const basePort = options.basePort;
    config = {
      storagePrefix: `.vfs_storage/test_${basePort}_`,
      gateway: 'zenoh_router',
      components: {
        zenoh_router: { type: 'zenoh_router', port: basePort },
        ...getSplitOpsComponents(basePort, basePort - 9),
        export: { type: 'export', protocol: 'https', port: basePort + 2, env: { NEIGHBORS: `http://127.0.0.1:${basePort}` } },
        ux:     { type: 'ux',     protocol: 'https', port: basePort - 6069, dist: 'ux/dist/test' }
      }
    };
  }

  // Config validation: Check for port conflicts before starting any components
  const portToComponent = new Map();
  for (const [id, cfg] of Object.entries(config.components)) {
    if (cfg.port) {
      if (portToComponent.has(cfg.port)) {
        throw new Error(`Configuration Error in profile "${profileKey}": Components "${portToComponent.get(cfg.port)}" and "${id}" both attempt to use port ${cfg.port}.`);
      }
      portToComponent.set(cfg.port, id);
    }
  }

  const capturedLogs = [];

  const { storagePrefix, components: componentMap, gateway, env = {} } = config;

  const sslDir = path.join(__dirname, '.ssl');
  const keyPath = path.join(sslDir, 'localhost-key.pem');
  const certPath = path.join(sslDir, 'localhost-cert.pem');
  const hasCerts = fs.existsSync(keyPath) && fs.existsSync(certPath);

  info(`[Orchestrator] Launching ${profileKey.toUpperCase()} Cluster (Log: ${globalLogLevel})...`);

  const ports = Object.fromEntries(Object.entries(componentMap).map(([id, cfg]) => [id, cfg.port]));
  if (ports.zenoh_router) {
    ports.zenoh_bridge = ports.zenoh_router + 1000;
  }

  const usedPorts = new Set();
  const launchedPortsSet = new Set();

  try {
    const portsToClean = Object.values(ports).filter(Boolean);
    if (portsToClean.length > 0) {
      const portList = portsToClean.map(p => `${p}/tcp`).join(' ');
      info(`[Orchestrator] Cleaning up ports: ${portList}`);
      execSync(`fuser -k ${portList} || true`, { stdio: 'ignore' });
    }
    // Native VFS cache cleanup
    const vfsStorageDir = '.vfs_storage';
    if (fs.existsSync(vfsStorageDir)) {
      try {
        const files = fs.readdirSync(vfsStorageDir);
        const prefixToken = storagePrefix.split('/').pop(); // Extract prefix (e.g., 'live_')
        for (const file of files) {
          if (prefixToken && file.startsWith(prefixToken)) {
            fs.rmSync(path.join(vfsStorageDir, file), { recursive: true, force: true });
          }
        }
      } catch (err) {
        warn(`[Orchestrator] Error reading cache directory: ${err.message}`);
      }
      fs.rmSync(path.join(vfsStorageDir, 'cli'), { recursive: true, force: true });
      fs.rmSync(path.join(vfsStorageDir, 'scratch-client'), { recursive: true, force: true });
    }
    execSync('sleep 1');
  } catch (e) {}

  for (const port of Object.values(ports).filter(Boolean)) {
    if (await isPortInUse(port)) {
      usedPorts.add(port);
    }
  }

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
    const uxProto = componentMap.ux.protocol || 'https';
    const uxPort = componentMap.ux.port;
    const gPort = componentMap.ux.gateway_port || gatewayPort;
    gatewayUrl = `${uxProto}://localhost:${uxPort}?gateway=${gPort}`;
  } else if (componentMap.export) {
    const expProto = componentMap.export.protocol || 'https';
    const expPort = componentMap.export.port;
    gatewayUrl = `${expProto}://localhost:${expPort}`;
  } else if (gatewayNode) {
    gatewayUrl = `http://localhost:${gatewayPort}`;
  }

  const createOpsConfig = (binaryPath, cfg, key) => {
    const useSsl = cfg.protocol === 'https';
    const neighborsCsv = cfg.neighbors ? cfg.neighbors.join(',') : '';
    return {
      name: `${path.basename(binaryPath)} (${cfg.port})`,
      command: binaryPath,
      args: [String(cfg.port), cfg.storage],
      cwd: __dirname,
      env: { 
          ...process.env, 
          LOG_LEVEL: globalLogLevel,
          PORT: String(cfg.port),
          PEER_ID: cfg.peer_id,
          SSL_CERT_PATH: (useSsl && hasCerts) ? certPath : '',
          SSL_KEY_PATH: (useSsl && hasCerts) ? keyPath : '',
          NEIGHBORS: neighborsCsv,
          ...env,
          ...cfg.env
      }
    };
  };

  const componentConfigs = {
    zenoh_router: (cfg) => {
      const multicastAddress = env.ZENOH_MULTICAST_ADDRESS;
      const routerArgs = ['-l', `tcp/0.0.0.0:${cfg.port}`];
      const bridgeArgs = [
        '-e', `tcp/127.0.0.1:${cfg.port}`,
        '--ws-port', String(cfg.websocket_port),
        '--no-multicast-scouting'
      ];
      
      if (multicastAddress) {
        routerArgs.push('--cfg', `scouting/multicast/address:"${multicastAddress}"`);
      } else {
        routerArgs.push('--cfg', 'scouting/multicast/enabled:false');
      }

      const home = os.homedir();
      return [
        {
          name: `Zenoh Router (${cfg.port})`,
          command: path.join(home, '.cargo/bin/zenohd'),
          args: routerArgs,
          cwd: __dirname,
          env: { ...process.env, ...env }
        },
        {
          name: `Zenoh Bridge (${cfg.port})`,
          command: path.join(home, '.cargo/bin/zenoh-bridge-remote-api'),
          args: bridgeArgs,
          cwd: __dirname,
          env: { ...process.env, ...env }
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
      const neighborsCsv = cfg.neighbors ? cfg.neighbors.join(',') : '';
      return {
        name: `Export Node (${cfg.port})`,
        command: 'node',
        args: ['--stack-size=65536', 'geo/export_service.js'],
        cwd: __dirname,
        env: { 
            ...process.env, 
            LOG_LEVEL: globalLogLevel,
            PORT: String(cfg.port),
            VFS_ID: cfg.vfs_id,
            NEIGHBORS: neighborsCsv,
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

      const gatewayUrl = `${cfg.protocol || 'https'}://localhost:${cfg.port}?gateway=${cfg.gateway_port}`;

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

  const components = [];
  for (const [id, cfg] of Object.entries(componentMap)) {
    const res = componentConfigs[cfg.type](cfg, id);
    const subConfigs = Array.isArray(res) ? res : [res];
    for (const proc of subConfigs) {
      let procPort = cfg.port;
      if (proc.name.startsWith('Zenoh Bridge')) {
        procPort = cfg.port + 1000;
      }
      if (procPort && usedPorts.has(procPort)) {
        info(`[Orchestrator] Process "${proc.name}" is already running on port ${procPort}. Skipping launch.`);
        continue;
      }
      if (procPort) {
        launchedPortsSet.add(procPort);
      }
      components.push(proc);
    }
  }

  const processes = new Map();
  let shuttingDown = false;

  const shutdown = async () => {
    if (shuttingDown) return;
    shuttingDown = true;
    info(`\n[Orchestrator] Shutting down ${profileKey.toUpperCase()} cluster...`);
    for (const [name, child] of processes) {
      info(`[Orchestrator] Killing ${name}...`);
      try {
        process.kill(-child.pid, 'SIGTERM');
      } catch (e) {
        try { child.kill('SIGTERM'); } catch (err) {}
      }
    }
    await new Promise(r => setTimeout(r, 200));
    for (const [name, child] of processes) {
      try {
        process.kill(-child.pid, 'SIGKILL');
      } catch (e) {
        try { child.kill('SIGKILL'); } catch (err) {}
      }
    }
    try {
        const portsToKill = Array.from(launchedPortsSet).map(p => `${p}/tcp`).join(' ');
        if (portsToKill.trim()) {
            execSync(`fuser -k ${portsToKill} || true`, { stdio: 'ignore' });
        }
    } catch (e) {}
  };

  const launch = (component) => {
    info(`[Orchestrator] Starting ${component.name}...`);
    const child = spawn(component.command, component.args, {
      cwd: component.cwd,
      env: component.env,
      stdio: 'pipe',
      detached: true
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

  const routerComponents = components.filter(c => c.name.startsWith('Zenoh Router') || c.name.startsWith('Zenoh Bridge'));
  const otherComponents = components.filter(c => !c.name.startsWith('Zenoh Router') && !c.name.startsWith('Zenoh Bridge'));

  // 1. Launch router components first
  routerComponents.forEach(launch);

  // 2. Ensure Zenoh Router is up (either pre-existing or just launched)
  if (ports.zenoh_router) {
    info(`[Orchestrator] Ensuring Zenoh Router is up and listening on port ${ports.zenoh_router}...`);
    let routerReady = false;
    for (let i = 0; i < 50; i++) {
      if (await isPortInUse(ports.zenoh_router)) {
        routerReady = true;
        break;
      }
      await new Promise(r => setTimeout(r, 200));
    }
    if (!routerReady) {
      const errorMsg = `CRITICAL: Zenoh Router on port ${ports.zenoh_router} failed to start or respond.`;
      error(errorMsg);
      await shutdown();
      throw new Error(errorMsg);
    }
    info(`[Orchestrator] Zenoh Router is up and listening.`);
  }

  // 3. Launch remaining components
  otherComponents.forEach(launch);

  // Verify that all launched processes stay up
  await new Promise(r => setTimeout(r, 1000));
  for (const component of components) {
    const child = processes.get(component.name);
    if (!child || child.exitCode !== null) {
      const code = child ? child.exitCode : 'unknown';
      const errorMsg = `CRITICAL: Component "${component.name}" exited prematurely during launch (exit code: ${code}).`;
      error(errorMsg);
      await shutdown();
      throw new Error(errorMsg);
    }
  }

  if (ports.export) {
      info(`[Orchestrator] Waiting for Export Node on port ${ports.export}...`);
      let exportReady = false;
      const protocol = (componentMap.export.protocol === 'https' && hasCerts) ? 'https' : 'http';
      for (let i = 0; i < 50; i++) {
        try {
          const url = `${protocol}://localhost:${ports.export}/health`;
          const options = {};
          if (protocol === 'https') options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
          const resp = await fetch(url, options);
          if (resp.ok) { exportReady = true; break; }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 200));
      }
      if (!exportReady) warn(`[Orchestrator] Export Node port ${ports.export} not responding after 10s.`);
  }

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

    const profileKey = args.find(arg => !arg.startsWith('--'));
    if (!profileKey) {
        error(`[Orchestrator] Error: Target profile key is mandatory. Specify a profile (e.g., "dev/standard" or "test/standard").`);
        process.exit(1);
    }
    
    try {
        const sys = await launchSystem(profileKey, logLevel);
        process.on('SIGINT', () => sys.stop().then(() => process.exit(0)));
        process.on('SIGTERM', () => sys.stop().then(() => process.exit(0)));
    } catch (err) {
        error(`[Orchestrator] Failed to launch: ${err.message}`);
        process.exit(1);
    }
}
