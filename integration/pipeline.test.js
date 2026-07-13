import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import path from 'node:path';
import fs from 'node:fs/promises';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Full Mesh Pipeline (C++ Ops + JS Export)', async ({ t, vfs, mesh, readData }) => {
  const filename = 'pipeline_test_result.pdf';

  t.after(async () => {
    console.log('[Test Pipeline] Cleaning up...');
    await fs.rm(path.resolve(filename), { force: true }).catch(() => {});
    console.log('[Test Pipeline] Cleanup complete.');
  });

  await t.test(
    'should execute full structured pipeline across multiple language nodes',
    async () => {
      const hex = new Selector('jot/Hexagon/diameter', { diameter: 50 }).withOutput('$out');
      const offset = new Selector('jot/offset', { diameter: 5.0, $in: hex }).withOutput('$out');
      const outline = new Selector('jot/outline', { $in: offset }).withOutput('$out');
      const pipeline = new Selector('jot/pdf', { path: filename, $in: outline }).withOutput('$out');

      console.log('[Test Pipeline] Requesting structured export...');
      const bytes = await readData(pipeline);

      assert.ok(bytes && bytes.length > 100, 'Export should return valid PDF data');
      const header = new TextDecoder().decode(bytes.slice(0, 5));
      assert.strictEqual(header, '%PDF-', 'Result should be a valid PDF buffer');
      console.log(`[Test Pipeline] SUCCESS: Received ${bytes.length} byte PDF.`);
    }
  );

  await t.test('should reject underconstrained request to C++ op', async () => {
    vfs.addSchema('jot/Hexagon/diameter', {
      arguments: [
        { name: 'diameter', type: 'jot:number' }
      ]
    });

    console.log(
      '[Test Pipeline] Requesting underconstrained hexagon (no diameter)...'
    );
    try {
      // Use a Selector with empty parameters to trigger 'Missing required parameter'
      await vfs.read(new Selector('jot/Hexagon/diameter', {}), {});
      assert.fail('Should have been rejected locally');
    } catch (err) {
      assert.ok(err.message.includes("Missing required parameter"), `Expected validation error, got: ${err.message}`);
      console.log('[Test Pipeline] Local rejection SUCCESS:', err.message);
    }
  });

  await t.test('should reject stale request at C++ node', async () => {
    const past = Date.now() - 5000;
    console.log('[Test Pipeline] Sending stale query to C++ node via Zenoh...');

    const selector = new Selector('jot/Hexagon/diameter', { diameter: 10 });
    try {
      await mesh.readSelector(selector, { expiresAt: past });
      assert.fail('Should have been rejected');
    } catch (err) {
      assert.ok(err.message.includes('expired') || err.message.includes('408'), `Expected expiration error, got: ${err.message}`);
      console.log('[Test Pipeline] Remote C++ TTL rejection SUCCESS:', err.message);
    }
  });
});
