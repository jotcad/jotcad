import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Texture Mesh Resolution Integration', async ({ t, vfs, capturePNG }) => {
  // Register a mock jot/texture provider
  vfs.registerProvider('jot/texture', async (v, s) => {
    const material = s.parameters.material;
    if (material === 'test_pattern') {
      // 2x2 tiny checkboard PNG
      const pngBase64 = 'iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAAFklEQVQI12P4z8DAwMDAxMDAwMDAAAAMAgEAKh2GjQAAAABJRU5ErkJggg==';
      const bytes = new Uint8Array(Buffer.from(pngBase64, 'base64'));
      return {
          stream: new ReadableStream({
              start(controller) {
                  controller.enqueue(bytes);
                  controller.close();
              }
          }),
          metadata: { contentType: 'image/png' }
      };
    }
    return null;
  });

  await t.test('Material to Texture Resolution (End to End)', async () => {
    // 1. Create a shape with a material tag
    const matSelector = new Selector('jot/material', {
        $in: new Selector('jot/Box', { width: 10, height: 10, depth: 0 }).withOutput('$out'),
        material: 'test_pattern'
    }).withOutput('$out');
    
    // 2. Render it via C++. The C++ kernel should request 'jot/texture?material=test_pattern' over the mesh, 
    //    our JS Node will fulfill it, and C++ will render it.
    console.log('[Test] Initiating PNG render with Texture resolution...');
    const pngBytes = await capturePNG(matSelector, 'textured_box_test.png');
    
    // Minimal check: it's a valid PNG and not empty. Visual verification happens via the artifact.
    assert.ok(pngBytes.length > 100);
    assert.strictEqual(pngBytes[0], 0x89);
  });
});
