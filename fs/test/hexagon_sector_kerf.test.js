import test from 'node:test';
import assert from 'node:assert';
import http from 'node:http';
import { VFS, DiskStorage, MeshLink, registerVFSRoutes, Selector } from '../src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import { launchOpsNode } from './ops_helper.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const OPS_PATH = path.resolve(__dirname, '../../geo/bin/ops');
const PORT_OPS = 9301;
const STORAGE_OPS = path.resolve('.vfs_storage_sector_ops');
const STORAGE_CLIENT = path.resolve('.vfs_storage_sector_client');

test('Complex Mesh Expression: Hexagon Sector with Kerf', async (t) => {
  const PORT_CLIENT = 9302;
  let opsNode, vfs, mesh, server;

  t.before(async () => {
    await fs.rm(STORAGE_OPS, { recursive: true, force: true }).catch(() => {});
    await fs.rm(STORAGE_CLIENT, { recursive: true, force: true }).catch(() => {});
    
    console.log('[Test Sector] Launching C++ Native Node...');
    opsNode = await launchOpsNode(OPS_PATH, PORT_OPS, STORAGE_OPS);
    
    console.log('[Test Sector] Starting Node.js Test Client...');
    vfs = new VFS({
      id: 'sector-js-node',
      storage: new DiskStorage(STORAGE_CLIENT)
    });
    await vfs.init();
    
    mesh = new MeshLink(vfs, [`http://localhost:${PORT_OPS}`], {
        localUrl: `http://localhost:${PORT_CLIENT}`
    });

    server = http.createServer();
    registerVFSRoutes(vfs, server, '', mesh);
    await new Promise(resolve => server.listen(PORT_CLIENT, '0.0.0.0', resolve));

    await mesh.start();
  });

  t.after(async () => {
    console.log('[Test Sector] Cleaning up...');
    if (opsNode) await opsNode.stop();
    if (mesh) await mesh.stop();
    if (server) await new Promise(resolve => server.close(resolve));
    await vfs.close();
    await fs.rm(STORAGE_OPS, { recursive: true, force: true }).catch(() => {});
    await fs.rm(STORAGE_CLIENT, { recursive: true, force: true }).catch(() => {});
  });

  await t.test(
    'should resolve nested mesh expression for kerfed sector',
    async () => {
      // THE EXPRESSION:
      // pdf(offset(loop(group(origin, nth(points(hexagon), 0), nth(points(hexagon), 1))), diameter=5))

      const hexagon = new Selector('jot/Hexagon/diameter', { diameter: 200 }).withOutput('$out');
      const points = new Selector('jot/eachPoint', { $in: hexagon }).withOutput('$out');
      const point0 = new Selector('jot/nth', { $in: points, index: [0] }).withOutput('$out');
      const point1 = new Selector('jot/nth', { $in: points, index: [1] }).withOutput('$out');
      const origin = new Selector('jot/nth', { 
        $in: new Selector('jot/eachPoint', { 
          $in: new Selector('jot/Box', { width: 1, height: 1, depth: 1 }).withOutput('$out') 
        }).withOutput('$out'), 
        index: [0] 
      }).withOutput('$out');
      const group = new Selector('jot/group', { $in: origin, shapes: [point0, point1] }).withOutput('$out');
      const loop = new Selector('jot/loop', { $in: group }).withOutput('$out');
      const offset = new Selector('jot/offset', { $in: loop, diameter: 5.0 }).withOutput('$out');
      const pdf = new Selector('jot/pdf', { $in: offset, path: 'sector.pdf' }).withOutput('$out');

      console.log('[Test Sector] Requesting complex expression...');
      const pdfData = await vfs.readData(pdf.withOutput('file'));
      assert.ok(pdfData, 'Should return PDF data');
      
      // Check for PDF magic number
      const header = new TextDecoder().decode(pdfData.slice(0, 4));
      assert.strictEqual(header, '%PDF', 'Result should be a valid PDF buffer');

      console.log(`[Test Sector] SUCCESS: Generated ${pdfData.length} byte kerfed sector PDF.`);
    }
  );
});
