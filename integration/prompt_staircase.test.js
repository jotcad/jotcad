import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, Selector } from '../fs/src/index.js';
import { JotParser } from '../jot/src/parser.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { decodeGeometry } from '../jot/src/index.js';
import { launchSystem } from '../orchestrator.js';
import { generatePromptPrefix } from '../ux/src/lib/prompt_generator.js';
import { captureAndVerifyPNG } from './png_helper.js';

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

test('Prompt Generation and Staircase Execution Integration', { timeout: 120000 }, async (t) => {
    // 1. Launch the TEST system
    const sys = await launchSystem('test/standard');
    const CPP_PORT = sys.ports.ops;

    // Initialize VFS & Mesh Link
    const nodeVFS = new VFS({ id: 'prompt-test-node' });
    const mesh = new MeshLink(nodeVFS, [`http://localhost:${sys.ports.zenoh_router}`]);
    await nodeVFS.init();

    try {
        console.log('[Test] Connecting Node.js to C++...');
        await mesh.start();

        // Wait for Ops Node to see us
        const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
        await waitForMeshNodes(nodeVFS, ['geo-ops-node']);

        // 2. Fetch Catalog (sys/schema)
        console.log('[Test] Subscribing to sys/schema...');
        let catalogReceived = null;
        nodeVFS.events.on('notify', (selector, payload) => {
            if (selector.path === 'sys/schema') {
                catalogReceived = payload;
            }
        });

        await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);

        let attempts = 0;
        while (!catalogReceived && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(catalogReceived, 'Should have received schema catalog');
        const catalog = catalogReceived.catalog;

        // 3. Test: Translate Schema into LLM Prompt Prefix
        console.log('[Test] Generating LLM Prompt Prefix...');
        const prompt = generatePromptPrefix(catalog);

        // Verify prompt rules & updated metadata are present
        assert.ok(prompt.includes('# JOTCAD DSL CODING INSTRUCTIONS'), 'Prompt should contain heading');
        assert.ok(prompt.includes('Box'), 'Prompt should describe Box');
        assert.ok(prompt.includes('cut'), 'Prompt should describe cut');
        assert.ok(prompt.includes('at'), 'Prompt should describe at');
        assert.ok(prompt.includes('Fuse'), 'Prompt should describe Fuse constructor');
        assert.ok(prompt.includes('cube'), 'Box synonyms should be present');
        assert.ok(prompt.includes('subtract'), 'cut synonyms should be present');
        assert.ok(prompt.includes('align'), 'at synonyms should be present');
        assert.ok(prompt.includes('Implicitly bound subject'), 'Implicit binding description should be present');
        
        console.log('✔ Prompt Prefix generated and verified successfully');

        // 4. Simulate LLM translating "build a 3 meter staircase"
        console.log('[Test] Evaluating simulated 3 meter staircase script...');
        const staircaseScript = `
steps = Box(1.5, 0.4, 0.4).move([0 .. 2.7 by 0.3 inc do $ => [0, $, $]])
Fuse(steps) -> $out
        `.trim();

        // 5. Compile and evaluate the generated JOT code
        const compiler = new JotCompiler(nodeVFS);
        const parser = new JotParser();

        // Register the necessary operators dynamically from the discovered catalog
        compiler.registerOperator('Box', { path: 'jot/Box', schema: catalog['jot/Box'] });
        compiler.registerOperator('move', { path: 'jot/move', schema: catalog['jot/move'] });
        compiler.registerOperator('fuse', { path: 'jot/fuse', schema: catalog['jot/fuse'] });
        compiler.registerOperator('Fuse', { path: 'jot/Fuse', schema: catalog['jot/Fuse'] });

        const ast = parser.parse(staircaseScript);
        const results = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
        
        assert.strictEqual(results.length, 1, 'Should produce a single output bundle');
        const staircaseSelector = results[0].selector;

        // 6. Read and Verify the staircase geometry
        const response = await nodeVFS.read(staircaseSelector);
        assert.ok(response, 'Should return a valid response from VFS');

        const resultBytes = await consumeBytes(response.stream);
        const result = JSON.parse(new TextDecoder().decode(resultBytes));
        
        assert.ok(result.geometry, 'Fused staircase result should have a single flat geometry');
        
        const geoResponse = await nodeVFS.read(result.geometry);
        const geoBytes = await consumeBytes(geoResponse.stream);
        const geo = decodeGeometry(new TextDecoder().decode(geoBytes));

        assert.ok(geo.vertices.length > 0, 'Geometry should contain vertices');
        assert.ok(geo.triangles.length > 0, 'Geometry should contain triangles');

        // Check height coordinates
        let maxZ = -Infinity;
        let minZ = Infinity;
        for (const v of geo.vertices) {
            if (v.z > maxZ) maxZ = v.z;
            if (v.z < minZ) minZ = v.z;
        }
        console.log(`[Test Staircase] Fused Min Z: ${minZ}, Fused Max Z: ${maxZ}`);
        // Box height is [-0.2, 0.2]. 
        // Max moved Z is 2.7. So max vertex Z is 2.7 + 0.2 = 2.9.
        // Min moved Z is 0.0. So min vertex Z is 0.0 - 0.2 = -0.2.
        // Total height span = 2.9 - (-0.2) = 3.1 meters!
        const span = maxZ - minZ;
        assert.ok(span > 3.09 && span < 3.11, `Total vertical span of the staircase (${span}) should be exactly 3.1 meters`);

        // 7. Render and save PNG snapshot for visual inspection
        console.log('[Test] Capturing visual rendering to actual/staircase.png...');
        await captureAndVerifyPNG(nodeVFS, staircaseSelector, 'staircase.png');
        console.log('✔ Staircase rendered and saved to integration/actual/staircase.png');

    } finally {
        mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
