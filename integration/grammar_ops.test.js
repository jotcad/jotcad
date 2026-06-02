import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, registerVFSRoutes, DiskStorage, Selector } from '../fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import http from 'node:http';
import { launchSystem } from '../orchestrator.js';

async function consumeBytes(stream) {
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
    }
    const len = chunks.reduce((acc, c) => acc + c.length, 0);
    const bytes = new Uint8Array(len);
    let offset = 0;
    for (const chunk of chunks) { bytes.set(chunk, offset); offset += chunk.length; }
    return bytes;
}

test('Geometric Grammar Integration', { timeout: 30000 }, async (t) => {
  let sys;
  let vfs;
  let mesh;
  let server;

  t.after(async () => {
    console.log('[Test Grammar] Cleaning up...');
    if (server) server.close();
    if (vfs) await vfs.close();
    if (sys) await sys.stop();
  });

  // 1. Launch the TEST system
  sys = await launchSystem('test/standard');
  const OPS_URL = `http://localhost:${sys.ports.ops}`;

  // 2. Start JS Node
  console.log('[Test Grammar] Starting Node.js Test Client...');
  vfs = new VFS({
    id: 'grammar-js-node',
    storage: new DiskStorage('.vfs_storage_grammar-js'),
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
      const points = new Selector('jot/points', { $in: hexagon }).withOutput('$out');
      const point0 = new Selector('jot/nth', { $in: points, index: [0] }).withOutput('$out');
      const point1 = new Selector('jot/nth', { $in: points, index: [1] }).withOutput('$out');
      const origin = new Selector('jot/points', {
        $in: new Selector('jot/Box', { width: 0, height: 0, depth: 0 }).withOutput('$out'),
      }).withOutput('$out'); 

      const group = new Selector('jot/group', { $in: origin, shapes: [point0, point1] }).withOutput('$out');
      const sector = new Selector('jot/loop', { $in: group });

      console.log('[Test Grammar] Requesting complex grammar sector...');
      const { stream } = await vfs.read(sector.withOutput('$out'));
      const shape = JSON.parse(new TextDecoder().decode(await consumeBytes(stream)));
      
      console.log('[Test Grammar] Shape received, requesting geometry:', shape.geometry);
      const { stream: geoStream } = await vfs.read(shape.geometry);
      const geoText = new TextDecoder().decode(await consumeBytes(geoStream));

      assert.ok(geoText, 'Result should be defined');

      // Validation: JOT format starts with "V <count>\n<x> <y> <z>\n..."
      const lines = geoText.trim().split('\n');
      let vCount = 0;
      let sCount = 0;
      
      const vMatch = lines[0].match(/^V (\d+)/);
      if (vMatch) vCount = parseInt(vMatch[1]);

      for (const line of lines) {
        if (line.startsWith('S ')) {
          const sMatch = line.match(/^S (\d+)/);
          if (sMatch) sCount = parseInt(sMatch[1]);
        }
      }

      console.log(
        `[Test Grammar] Result has ${vCount} vertices and ${sCount} segments.`
      );
      assert.strictEqual(
        vCount,
        3,
        'Should have 3 vertices (origin + 2 hex points)'
      );
      assert.strictEqual(sCount, 3, 'Should have 3 segments (triangle)');

      // Origin should be among the vertices
      assert.ok(
        geoText.includes('0 0 0') || geoText.includes('0.000000 0.000000 0.000000'),
        'One vertex should be origin (0,0,0)'
      );
    }
  );
});
