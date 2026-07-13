import assert from 'node:assert/strict';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Sequence Rotation and Section Integration Tests', async ({
  t,
  evaluate,
  readOutput,
  captureOutputPNG
}) => {
  await t.test('Box(1, 10).rz([by 1/8]).fuse()', async () => {
    const code = "Box(1, 10).rz([by 1/8]).fuse() -> $out";
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const results = await evaluate(code);
        
        // Read and verify the shape metadata
        const shape = await readOutput(results);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of the fused shape
        await captureOutputPNG(results, 'sequence_rotation_fuse_result.png');
        console.log("SUCCESS: Box sequence rotation and fuse integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 1).rz([by 1/8]).section()', async () => {
    const code = "Box(1, 10, 1).rz([by 1/8]).section() -> $out";
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const results = await evaluate(code);

        // Read and verify the shape metadata
        const shape = await readOutput(results);
        console.log("Shape tags:", shape.tags);
        assert.strictEqual(shape.tags.type, 'group', "Expected result tag type to be 'group'");
        assert.ok(shape.components && shape.components.length > 0, "Expected result to be a group with child components");
        assert.strictEqual(shape.components[0].tags.type, 'surface', "Expected child components to have 'surface' tag type");
        assert.ok(shape.components[0].geometry, "Expected child components to have a geometry CID");

        // Save PNG capture of the sectioned shape
        await captureOutputPNG(results, 'sequence_rotation_section_result.png');
        console.log("SUCCESS: Box sequence rotation and section integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10).rz([by 1/8]).outline().fill(rule="odd")', async () => {
    const code = 'Box(1, 10).rz([by 1/8]).outline().fill(rule="odd") -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const results = await evaluate(code);

        // Read and verify the shape metadata
        const shape = await readOutput(results);
        console.log("Shape tags:", shape.tags);
        assert.strictEqual(shape.tags.type, 'surface', "Expected result tag type to be 'surface'");
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of the filled shape
        await captureOutputPNG(results, 'sequence_rotation_fill_result.png');
        console.log("SUCCESS: Box sequence rotation outline and fill integration test passed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 2).rz([by 1/8]).section().fuse().ez(5)', async () => {
    const code = 'Box(1, 10, 2).rz([by 1/8]).section().fuse().ez(5) -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const results = await evaluate(code);

        // Read and verify the shape metadata
        const shape = await readOutput(results);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of this shape
        await captureOutputPNG(results, 'sequence_rotation_extruded_fuse_result.png');
        console.log("SUCCESS: Box sequence rotation section fuse and extrude test completed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });

  await t.test('Box(1, 10, 2).rz([by 1/8]).section().fuse().outline()', async () => {
    const code = 'Box(1, 10, 2).rz([by 1/8]).section().fuse().outline() -> $out';
    console.log(`[Test] Evaluating: ${code}`);
    
    try {
        const results = await evaluate(code);

        // Read and verify the shape metadata
        const shape = await readOutput(results);
        console.log("Shape tags:", shape.tags);
        assert.ok(shape.geometry, "Expected result to have a geometry CID");

        // Save PNG capture of this shape
        await captureOutputPNG(results, 'sequence_rotation_outline_result.png');
        console.log("SUCCESS: Box sequence rotation section fuse and outline test completed.");
    } catch (e) {
        console.error('[Test] Evaluation failed:', e);
        throw e;
    }
  });
});
