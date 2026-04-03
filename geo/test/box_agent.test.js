import { test } from 'node:test';
import assert from 'node:assert';
import { VFS, registerVFSRoutes } from '../../fs/src/index.js';
import { Dispatcher } from '../src/dispatcher.js';
import http from 'node:http';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));

test('C++ Box Agent Evaluator', { timeout: 15000 }, async (t) => {
  const vfs = new VFS({ id: 'hub' });
  const server = http.createServer();
  registerVFSRoutes(vfs, server, '/vfs');

  const port = await new Promise((resolve) =>
    server.listen(0, '127.0.0.1', () => resolve(server.address().port))
  );
  const hubUrl = `http://127.0.0.1:${port}/vfs`;

  // 1. Setup Dispatcher
  const dispatcher = new Dispatcher(vfs, {
    hubUrl,
    binDir: path.join(__dirname, '..', 'bin'),
  });
  dispatcher.register('shape/box', 'box_agent');
  dispatcher.start();

  await t.test('demand for a box coordinate triggers C++ evaluator', async () => {
    const boxCoord = 'shape/box';
    const boxParams = { width: 10, height: 20, depth: 5 };

    console.log('[Test] Requesting box...');
    // This blocks until the C++ agent is spawned and fulfills it
    const stream = await vfs.read(boxCoord, boxParams);

    let shapeJson = '';
    const decoder = new TextDecoder();
    for await (const chunk of stream) {
      shapeJson += decoder.decode(chunk, { stream: true });
    }
    shapeJson += decoder.decode();

    const shape = JSON.parse(shapeJson);
    console.log('[Test] Received Shape:', shape);

    assert.strictEqual(shape.parameters.width, 10);
    assert.strictEqual(shape.tags.type, 'box');
    assert.ok(shape.geometry.startsWith('vfs:/geo/mesh'));

    // Verify linked geometry exists
    const geoStream = await vfs.read('geo/mesh', boxParams);
    let geoText = '';
    for await (const chunk of geoStream) {
      geoText += decoder.decode(chunk, { stream: true });
    }
    geoText += decoder.decode();
    
    console.log('[Test] Received Geometry (first 50 chars):', geoText.slice(0, 50));
    assert.ok(geoText.includes('v 10.0000000000 20.0000000000 5.0000000000'));
  });

  // Cleanup
  server.close();
  await vfs.close();
});
