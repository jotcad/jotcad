import test from 'node:test';
import assert from 'node:assert';
import { spawnOpsNode } from './ops_helper.js';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage, Selector } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');

const OPS_PORT = 9200;
const OPS_URL = `http://localhost:${OPS_PORT}`;
const STORAGE_OPS = path.resolve('.vfs_storage_grammar-ops');
const STORAGE_JS = path.resolve('.vfs_storage_grammar-js');

test('Geometric Grammar Integration', { timeout: 30000 }, async (t) => {
  let opsProcess;
  let vfs;
  let mesh;
  let server;

  t.after(async () => {
    console.log('[Test Grammar] Cleaning up (PROCESS ONLY)...');
    if (opsProcess) opsProcess.kill();
    if (server) server.close();
    if (vfs) await vfs.close();
  });

  // 1. Start Native C++ Node
  console.log('[Test Grammar] Launching C++ Native Node...');
  opsProcess = await spawnOpsNode(
    OPS_PATH,
    [OPS_PORT.toString(), STORAGE_OPS],
    OPS_PORT,
    {
      env: { PEER_ID: 'grammar-ops-node' },
      stdio: 'pipe', // Pipe output to avoid noise but let helper monitor
    }
  );

  // 2. Start JS Node
  console.log('[Test Grammar] Starting Node.js Test Client...');
  vfs = new VFS({
    id: 'grammar-js-node',
    storage: new DiskStorage(STORAGE_JS),
  });
  mesh = new MeshLink(vfs, [OPS_URL], { localUrl: 'http://localhost:9201' });
  server = http.createServer();
  registerVFSRoutes(vfs, server, '', mesh);

  await new Promise((resolve) => server.listen(9201, '0.0.0.0', resolve));
  await vfs.init();
  await mesh.start();

  await t.test(
    'should construct a hexagon sector using nested mesh operations',
    async () => {
      // THE EXPRESSION:
      // loop(group(origin, nth(points(hexagon), index=0), nth(points(hexagon), index=1)))
      
      const hexagon = new Selector('jot/Hexagon/full', { diameter: 200 }).withOutput('$out');
      const points = new Selector('jot/eachPoint', { $in: hexagon }).withOutput('$out');
      const point0 = new Selector('jot/nth', { $in: points, index: [0] }).withOutput('$out');
      const point1 = new Selector('jot/nth', { $in: points, index: [1] }).withOutput('$out');
      const origin = new Selector('jot/eachPoint', {
        $in: new Selector('jot/Box', { width: 0, height: 0, depth: 0 }).withOutput('$out'),
      }).withOutput('$out'); 
      const origin0 = new Selector('jot/at', { $in: origin, target: point0, op: new Selector('jot/Box', { width: 1, height: 1, depth: 1 }).withOutput('$out') }).withOutput('$out');

      const group = new Selector('jot/group', { $in: origin0, shapes: [point0, point1] }).withOutput('$out');
      const sector = new Selector('jot/loop', { $in: group });

      console.log('[Test Grammar] Requesting complex grammar sector...');
      const geoText = await vfs.readText(sector.withOutput('$out'));

      assert.ok(geoText, 'Result should be defined');

      // Validation:
      const lines = geoText.trim().split('\n');
      const vCount = lines.filter((l) => l.startsWith('v ')).length;
      
      let sCount = 0;
      for (const line of lines) {
        if (line.startsWith('s ')) {
          // Format: "s v1 v2 v3 v4..." where each pair is a segment
          const parts = line.split(' ').filter(p => p !== 's' && p.trim() !== '');
          sCount += (parts.length / 2);
        }
      }

      console.log(
        `[Test Grammar] Result has ${vCount} vertices and ${sCount} segments.`
      );
      console.log('--- GEOMETRY START ---');
      console.log(geoText);
      console.log('--- GEOMETRY END ---');

      assert.strictEqual(
        vCount,
        3,
        'Should have 3 vertices (origin + 2 hex points)'
      );
      assert.strictEqual(sCount, 3, 'Should have 3 segments (triangle)');

      // Origin should be among the vertices (allowing for -0.000000 formatting)
      assert.ok(
        geoText.includes('0.000000 0.000000 0.000000'),
        'One vertex should be origin (0,0,0)'
      );
    }
  );
});
