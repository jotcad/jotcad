import assert from 'node:assert';
import { decodeGeometry } from '../jot/src/index.js';
import { generatePromptPrefix } from '../ux/src/lib/prompt_generator.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Prompt Generation and Staircase Execution Integration', async ({
    vfs,
    mesh,
    evaluate,
    readData,
    readOutput,
    captureOutputPNG
}) => {
    // 1. Get the catalog from mesh
    const catalog = mesh.catalog;
    assert.ok(catalog, 'Catalog should be loaded by the harness');

    // 2. Test: Translate Schema into LLM Prompt Prefix
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

    // 3. Simulate LLM translating "build a 3 meter staircase"
    console.log('[Test] Evaluating simulated 3 meter staircase script...');
    const staircaseScript = `
steps = Box(1.5, 0.4, 0.4).move([0 .. 2.7 by 0.3 inc do $ => [0, $, $]])
Fuse(steps) -> $out
    `.trim();

    // 4. Compile and evaluate the generated JOT code
    const results = await evaluate(staircaseScript);
    assert.strictEqual(results.length, 1, 'Should produce a single output bundle');

    // 5. Read and Verify the staircase geometry
    const result = await readOutput(results);
    assert.ok(result.geometry, 'Fused staircase result should have a single flat geometry');
    
    const geoData = await readData(result.geometry);
    const geoStr = (typeof geoData === 'string') ? geoData : new TextDecoder().decode(geoData);
    const geo = decodeGeometry(geoStr);

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
    const span = maxZ - minZ;
    assert.ok(span > 3.09 && span < 3.11, `Total vertical span of the staircase (${span}) should be exactly 3.1 meters`);

    // 6. Render and save PNG snapshot for visual inspection
    console.log('[Test] Capturing visual rendering to actual/staircase.png...');
    await captureOutputPNG(results, 'staircase.png');
    console.log('✔ Staircase rendered and saved to integration/actual/staircase.png');
});
