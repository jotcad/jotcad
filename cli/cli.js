#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { parseArgs } from 'node:util';
import net from 'node:net';

import { VFS, DiskStorage, MeshLink, Selector } from '../fs/src/index.js';
import { UGCEngine } from '../ugc/src/index.js';

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

// Router Auto-Discovery
async function discoverGateway(explicitGateway) {
  if (explicitGateway) return explicitGateway;
  if (process.env.ZENOH_ROUTER_URL) return process.env.ZENOH_ROUTER_URL;
  if (process.env.VITE_VFS_URL) return process.env.VITE_VFS_URL;

  const candidatePorts = [9000, 9200, 9092];
  for (const port of candidatePorts) {
    if (await isPortOpen(port)) {
      return `http://127.0.0.1:${port}`;
    }
  }
  return 'http://127.0.0.1:9000';
}

async function main() {
  const options = {
    input: { type: 'string', short: 'i', multiple: true, default: [] },
    output: { type: 'string', short: 'o', multiple: true, default: [] },
    gateway: { type: 'string', short: 'g' },
    help: { type: 'boolean', short: 'h' }
  };

  let parsed;
  try {
    parsed = parseArgs({ options, allowPositionals: true });
  } catch (err) {
    console.error(`Error parsing arguments: ${err.message}`);
    process.exit(1);
  }

  if (parsed.values.help || parsed.positionals.length === 0) {
    console.log(`
JotCAD CLI - Command Line Interface for Jot Script Compilation & CAD Export

Usage:
  jotcad <input.jot> [options]

Options:
  -i, --input <name=val>     Input variables to bind (e.g. -i size=20)
  -o, --output <name=path>   Output file destinations (e.g. -o stl_file=part.stl)
  -g, --gateway <url>        Zenoh router gateway URL (auto-discovered if omitted)
  -h, --help                 Show this help menu

Examples:
  jotcad model.jot -i size=25 -o stl_file=cube.stl -o png_file=preview.png
`);
    process.exit(0);
  }

  const inputFile = path.resolve(parsed.positionals[0]);
  if (!fs.existsSync(inputFile)) {
    console.error(`Error: Input file '${inputFile}' does not exist.`);
    process.exit(1);
  }

  const scriptContent = fs.readFileSync(inputFile, 'utf-8');

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

  // Parse outputs: -o stl_file=part.stl -> { stl_file: 'part.stl' }
  const cliOutputs = {};
  const outputArgs = parsed.values.output || [];
  for (const arg of outputArgs) {
    const idx = arg.indexOf('=');
    if (idx !== -1) {
      const k = arg.slice(0, idx).trim();
      const v = arg.slice(idx + 1).trim();
      cliOutputs[k] = path.resolve(v);
    }
  }

  // 1. Discover Zenoh router
  const gatewayUrl = await discoverGateway(parsed.values.gateway);
  console.log(`[JotCAD CLI] Connecting mesh gateway: ${gatewayUrl}`);

  // 2. Initialize VFS and MeshLink
  const storageDir = path.join(__dirname, '.vfs_storage_cli');
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

  let attempts = 0;
  while (!catalogReceived && attempts < 50) {
    await new Promise(r => setTimeout(r, 100));
    attempts++;
  }

  if (catalogReceived) {
    console.log(`[JotCAD CLI] Loaded ${Object.keys(catalogReceived.catalog).length} operators from mesh.`);
  } else {
    console.warn('[JotCAD CLI] Warning: Catalog discovery timed out. Proceeding with local registries.');
  }

  // 4. Construct outputs schema
  const schema = {
    outputs: Object.keys(cliOutputs).reduce((acc, key) => {
      acc[key] = { type: 'jot:file' };
      return acc;
    }, {})
  };

  // 5. Evaluate script
  console.log('[JotCAD CLI] Evaluating script with inputs:', cliInputs);
  let results;
  try {
    results = await ugc.evaluate(scriptContent, cliInputs, schema);
  } catch (err) {
    console.error(`[JotCAD CLI] Compilation Error: ${err.message}`);
    await mesh.stop();
    process.exit(1);
  }

  // 6. Map and save output files
  for (const { port, selector } of results) {
    const targetFile = cliOutputs[port];
    if (!targetFile) continue;

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

  // 7. Clean Shutdown
  await mesh.stop();
  console.log('[JotCAD CLI] Done!');
}

main().catch(err => {
  console.error('[JotCAD CLI] Unhandled error:', err);
  process.exit(1);
});
