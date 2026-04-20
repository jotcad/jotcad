import test from 'node:test';
import assert from 'node:assert';
import { VFS, DiskStorage, MeshLink } from '../src/index.js';
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
  let opsNode, vfs, mesh;

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
    
    mesh = new MeshLink(vfs, [`http://localhost:${PORT_OPS}`]);
    await mesh.start();
  });

  t.after(async () => {
    console.log('[Test Sector] Cleaning up...');
    if (opsNode) await opsNode.stop();
    if (mesh) await mesh.stop();
    await vfs.close();
    await fs.rm(STORAGE_OPS, { recursive: true, force: true }).catch(() => {});
    await fs.rm(STORAGE_CLIENT, { recursive: true, force: true }).catch(() => {});
  });

  await t.test(
    'should resolve nested mesh expression for kerfed sector',
    async () => {
      // THE EXPRESSION:
      // pdf(offset(loop(group(origin, nth(points(hexagon), 0), nth(points(hexagon), 1))), diameter=5))

      const hexagon = { path: 'jot/Hexagon/full', parameters: { diameter: 200 } };
      const points = { path: 'jot/points', parameters: { $in: hexagon } };
      const point0 = { path: 'jot/nth', parameters: { $in: points, index: 0 } };
      const point1 = { path: 'jot/nth', parameters: { $in: points, index: 1 } };
      const origin = { path: 'jot/nth', parameters: { $in: { path: 'jot/points', parameters: { $in: { path: 'jot/Box', parameters: { width: 1, height: 1, depth: 1 } } } }, index: 0 } };
      const group = { path: 'jot/group', parameters: { $in: origin, shapes: [point0, point1] } };
      const loop = { path: 'jot/loop', parameters: { $in: group } };
      const offset = { path: 'jot/offset', parameters: { $in: loop, diameter: 5.0 } };
      const pdf = { path: 'jot/pdf', parameters: { $in: offset, path: 'sector.pdf' } };

      console.log('[Test Sector] Requesting complex expression...');
      const pdfData = await vfs.readData({
        ...pdf,
        parameters: { ...pdf.parameters, $path: 'sector.pdf' },
      });

      assert.ok(pdfData, 'Should return PDF data');
      
      // Check for PDF magic number
      const header = new TextDecoder().decode(pdfData.slice(0, 4));
      assert.strictEqual(header, '%PDF', 'Result should be a valid PDF buffer');

      console.log(`[Test Sector] SUCCESS: Generated ${pdfData.length} byte kerfed sector PDF.`);
    }
  );
});
