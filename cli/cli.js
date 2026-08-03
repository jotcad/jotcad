#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import net from 'node:net';

import { VFS, DiskStorage, MeshLink, Selector } from '../fs/src/index.js';
import { UGCEngine } from '../ugc/src/index.js';
import { PROFILES } from '../orchestrator.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Helper to check if a TCP port is open and listening locally
function isPortOpen(port, host = '127.0.0.1') {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    socket.setTimeout(200);
    socket.on('connect', () => {
      socket.destroy();
      resolve(true);
    });
    socket.on('error', () => resolve(false));
    socket.on('timeout', () => {
      socket.destroy();
      resolve(false);
    });
    socket.connect(port, host);
  });
}

// Router Discovery
async function discoverGateway(selectedProfile, explicitGateway) {
  if (explicitGateway) return explicitGateway;
  if (process.env.ZENOH_ROUTER_URL) return process.env.ZENOH_ROUTER_URL;
  if (process.env.VITE_VFS_URL) return process.env.VITE_VFS_URL;

  const profileKey = selectedProfile || process.env.JOTCAD_PROFILE;
  if (!profileKey) {
    console.error('Error: Target profile is mandatory. Specify --profile <dev|test|prod> (or set JOTCAD_PROFILE).');
    process.exit(1);
  }

  const profileMap = {
    'dev': 'live/standard',
    'test': 'test/standard',
    'prod': 'prod',
    'live/standard': 'live/standard',
    'test/standard': 'test/standard',
    'prod/standard': 'prod'
  };

  const resolvedProfile = profileMap[profileKey.toLowerCase()];
  if (!resolvedProfile) {
    console.error(`Error: Unknown profile "${profileKey}". Available options: dev, test, prod`);
    process.exit(1);
  }

  const profile = PROFILES[resolvedProfile];
  const routerPort = profile.components.zenoh_router.port;
  return `http://127.0.0.1:${routerPort}`;
}

async function main() {
  const options = {
    input: { type: 'string', short: 'i', multiple: true, default: [] },
    output: { type: 'string', short: 'o', multiple: true, default: [] },
    profile: { type: 'string', short: 'p' },
    gateway: { type: 'string', short: 'g' },
    eval: { type: 'string', short: 'e' },
    session: { type: 'string', short: 's', default: 'scratch/my_cad_session' },
    note: { type: 'string', short: 'm' },
    help: { type: 'boolean', short: 'h' }
  };

  let parsed;
  try {
    parsed = parseArgs({ options, allowPositionals: true });
  } catch (err) {
    console.error(`Error parsing arguments: ${err.message}`);
    process.exit(1);
  }

  if (parsed.values.help || (parsed.positionals.length === 0 && !parsed.values.eval)) {
    console.log(`
JotCAD CLI - Command Line Interface for Jot Script Compilation & CAD Export

Usage:
  jotcad <input.jot> [options]
  jotcad -e "<expression>" [options]

Options:
  -p, --profile <dev|test|prod> Target cluster profile (MANDATORY, or set JOTCAD_PROFILE)
  -e, --eval <script>        Jot script string expression to evaluate directly
  -i, --input <name=val>     Input variables to bind (e.g. -i size=20)
  -o, --output <name=path>   Output file destinations (e.g. -o stl_file=part.stl)
  -s, --session <dir>        Output session directory to track sequentially
  -m, --note <string>        Descriptive snapshot note for the session run
  -g, --gateway <url>        Override Zenoh router gateway URL
  -h, --help                 Show this help menu

Examples:
  jotcad -p dev -e "Box(25).stl() -> out;" -o out=box.stl
  jotcad model.jot -p dev -i size=25 -o stl_file=cube.stl -o png_file=preview.png
  echo "Box(25).stl() -> out;" | jotcad -p dev - -o out=box.stl
`);
    process.exit(0);
  }

  const profileKey = parsed.values.profile || process.env.JOTCAD_PROFILE;
  if (!profileKey) {
    console.error('Error: Target profile is mandatory. Specify --profile <dev|test|prod> (or set JOTCAD_PROFILE environment variable).');
    process.exit(1);
  }

  let scriptContent = '';
  if (parsed.values.eval) {
    scriptContent = parsed.values.eval;
  } else if (parsed.positionals[0] === '-') {
    scriptContent = await new Promise((resolve) => {
      let data = '';
      process.stdin.setEncoding('utf-8');
      process.stdin.on('data', chunk => data += chunk);
      process.stdin.on('end', () => resolve(data));
    });
  } else if (parsed.positionals.length > 0) {
    const inputFile = path.resolve(parsed.positionals[0]);
    if (!fs.existsSync(inputFile)) {
      console.error(`Error: Input file '${inputFile}' does not exist.`);
      process.exit(1);
    }
    scriptContent = fs.readFileSync(inputFile, 'utf-8');
  } else {
    console.error('Error: You must specify an input file, use "-" for stdin, or use -e/--eval.');
    process.exit(1);
  }

  // Parse inputs: -i size=20 -> { size: 20 }
  const cliInputs = {};
  const inputArgs = parsed.values.input || [];
  for (const arg of inputArgs) {
    const idx = arg.indexOf('=');
    if (idx !== -1) {
      const k = arg.slice(0, idx).trim();
      const v = arg.slice(idx + 1).trim();
      cliInputs[k] = isNaN(Number(v)) ? v : Number(v);
    }
  }

  // Parse outputs: -o stl_file:file=part.stl -> { stl_file: { type: 'jot:file', path: 'part.stl' } }
  const cliOutputs = {};
  const outputArgs = parsed.values.output || [];
  for (const arg of outputArgs) {
    const eqIdx = arg.indexOf('=');
    if (eqIdx !== -1) {
      const targetPath = arg.slice(eqIdx + 1).trim();
      const portSpec = arg.slice(0, eqIdx).trim();

      const colonIdx = portSpec.indexOf(':');
      let portName = portSpec;
      let portType = 'file'; // Default type

      if (colonIdx !== -1) {
        portName = portSpec.slice(0, colonIdx).trim();
        portType = portSpec.slice(colonIdx + 1).trim();
      }

      // Map short type names to canonical Jot types
      const canonicalType = portType === 'file' ? 'jot:file' : (portType === 'shape' ? 'jot:shape' : `jot:${portType}`);

      cliOutputs[portName] = {
        type: canonicalType,
        path: parsed.values.session ? path.basename(targetPath) : path.resolve(targetPath)
      };
    }
  }



  // 1. Discover Zenoh router
  const gatewayUrl = await discoverGateway(parsed.values.profile, parsed.values.gateway);
  console.log(`[JotCAD CLI] Connecting mesh gateway: ${gatewayUrl}`);

  // 2. Initialize VFS and MeshLink
  const storageDir = path.resolve('.vfs_storage/cli');
  const vfs = new VFS({ id: 'jotcad-cli', storage: new DiskStorage(storageDir) });
  await vfs.init();

  const mesh = new MeshLink(vfs, [gatewayUrl]);
  await mesh.start();

  // Initialize UGC Engine
  const ugc = new UGCEngine(vfs, mesh);
  await ugc.init();

  // 3. Sync operators catalog
  console.log('[JotCAD CLI] Syncing operator catalog from mesh...');
  let catalogReceived = null;
  vfs.events.on('notify', (selector, payload) => {
    if (selector.path === 'sys/schema') {
      catalogReceived = payload;
    }
  });

  await mesh.subscribe(new Selector('sys/schema'), Date.now() + 15000);

  // Wait for catalog to stabilize (size stops growing for 500ms)
  let prevCount = 0;
  let stableAttempts = 0;
  for (let i = 0; i < 50; i++) {
    await new Promise(r => setTimeout(r, 100));
    const currentCount = Object.keys(mesh.catalog || {}).length;
    if (currentCount > 0 && currentCount === prevCount) {
      stableAttempts++;
      if (stableAttempts >= 5) break;
    } else {
      stableAttempts = 0;
      prevCount = currentCount;
    }
  }

  const catalogCount = Object.keys(mesh.catalog || {}).length;
  if (catalogCount > 0) {
    console.log(`[JotCAD CLI] Loaded ${catalogCount} operators from mesh.`);
  } else {
    console.warn('[JotCAD CLI] Warning: Catalog discovery timed out. Proceeding with local registries.');
  }

  // 4. Session Mode Execution
  if (parsed.values.session) {
    const sessionDir = path.resolve(parsed.values.session);
    console.log(`[JotCAD CLI] Running inside session workspace: ${sessionDir}`);
    
    const session = ugc.createSession(sessionDir);
    const note = parsed.values.note || 'CLI Run';
    
    try {
      const { snapshotDir } = await session.createSnapshot(scriptContent, cliInputs, cliOutputs, note);
      console.log(`[JotCAD CLI] Snapshot compiled successfully: ${snapshotDir}`);
    } catch (err) {
      console.error(`[JotCAD CLI] Session Compile Error: ${err.message}`);
      await mesh.stop();
      process.exit(1);
    }
  } 
  // 5. Standard Mode Execution (Direct Single File Exports)
  else {
    const schema = {
      outputs: Object.entries(cliOutputs).reduce((acc, [portName, spec]) => {
        acc[portName] = { type: spec.type };
        return acc;
      }, {})
    };

    console.log('[JotCAD CLI] Evaluating script with inputs:', cliInputs);
    let results;
    try {
      results = await ugc.evaluate(scriptContent, cliInputs, schema);
    } catch (err) {
      console.error(`[JotCAD CLI] Compilation Error: ${err.message}`);
      await mesh.stop();
      process.exit(1);
    }

    for (const { port, selector } of results) {
      const spec = cliOutputs[port];
      if (!spec) continue;
      const targetFile = spec.path;

      console.log(`[JotCAD CLI] Resolving output port: ${port} -> ${targetFile}`);

      try {
        const streamResult = await vfs.readSelector(selector);
        if (streamResult && streamResult.stream) {
          const chunks = [];
          for await (const chunk of streamResult.stream) {
            chunks.push(chunk);
          }
          const bytes = Buffer.concat(chunks);
          fs.writeFileSync(targetFile, bytes);
          console.log(`[JotCAD CLI] Saved: ${targetFile} (${bytes.length} bytes)`);
        } else {
          console.error(`[JotCAD CLI] Failed to resolve output port ${port}`);
        }
      } catch (err) {
        console.error(`[JotCAD CLI] Error exporting ${port}: ${err.message}`);
      }
    }
  }

  // Clean Shutdown
  await mesh.stop();
  console.log('[JotCAD CLI] Done!');
}

main().catch(err => {
  console.error('[JotCAD CLI] Unhandled error:', err);
  process.exit(1);
});
