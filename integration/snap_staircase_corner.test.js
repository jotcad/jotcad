import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { decodeGeometry } from '../jot/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Snap Staircase Corner Integration Test', async ({
    compiler,
    parser,
    readData,
    readOutput,
    captureOutputPNG
}) => {
    // 4. Evaluate the JOT script which:
    //    a. Constructs a block with a staircase cut out of it.
    //    b. Snaps to a copy of itself rotated 90 degrees (0.25 turns) so that the stairs continue around the corner.
    //    Using 0.42 step depth/height with 0.4 translation to ensure manifold solid overlap.
    const script = `
    stairs = Box(2, 0.42, 0.42).move([[1, 0, -2.0], [1, 0.4, -1.6], [1, 0.8, -1.2], [1, 1.2, -0.8], [1, 1.6, -0.4]])
    staircase = Fuse(stairs)
    block = Box(4, 4, 4)
    
    b1 = block.snap(bottom(), bottom(), staircase.gap()).disjoint()
    b2 = b1.rotateZ(0.25)
    b3 = b1.rotateZ(0.5)
    b4 = b1.rotateZ(0.75)
    
    s2 = b2.snap(bottom(), top().tx(0.5), b1)
    s3 = b3.snap(bottom(), top().tx(0.5), s2)
    assembly = b4.snap(bottom(), top().tx(0.5), s3)
    
    b1 -> $b1
    b2 -> $b2
    s2 -> $s2
    s3 -> $s3
    assembly -> $assembly
    Fuse(assembly) -> $out
    `.trim();

    console.log('[Test] Evaluating JOT script for snap staircase corner...');
    const ast = parser.parse(script);
    const results = await compiler.evaluate(ast, {}, {
        outputs: {
            $b1: { type: 'jot:shape' },
            $b2: { type: 'jot:shape' },
            $s2: { type: 'jot:shape' },
            $s3: { type: 'jot:shape' },
            $assembly: { type: 'jot:shape' },
            $out: { type: 'jot:shape' }
        }
    });

    const b1Shape = await readOutput(results, '$b1');
    const b2Shape = await readOutput(results, '$b2');
    const s2Shape = await readOutput(results, '$s2');
    const s3Shape = await readOutput(results, '$s3');
    const assemblyShape = await readOutput(results, '$assembly');

    console.log('--- DEBUG b1 ---');
    console.log(JSON.stringify(b1Shape, null, 2));
    console.log('--- DEBUG b2 ---');
    console.log(JSON.stringify(b2Shape, null, 2));
    console.log('--- DEBUG s2 ---');
    console.log(JSON.stringify(s2Shape, null, 2));
    console.log('--- DEBUG s3 ---');
    console.log(JSON.stringify(s3Shape, null, 2));

    // Evaluate top() on s2
    const s2TopSel = Selector.fromObject({
        path: 'jot/top',
        parameters: {
            $in: results.find(r => r.port === '$s2').selector
        },
        output: '$out'
    });
    const s2TopShape = await readData(s2TopSel);
    console.log('--- DEBUG s2 top ---');
    console.log(JSON.stringify(s2TopShape, null, 2));

    // Evaluate top() on s3
    const s3TopSel = Selector.fromObject({
        path: 'jot/top',
        parameters: {
            $in: results.find(r => r.port === '$s3').selector
        },
        output: '$out'
    });
    const s3TopShape = await readData(s3TopSel);
    console.log('--- DEBUG s3 top ---');
    console.log(JSON.stringify(s3TopShape, null, 2));

    console.log('--- DEBUG assembly ---');
    console.log(JSON.stringify(assemblyShape, null, 2));

    // 5. Read and verify the geometry of the fused assembly
    const result = await readOutput(results, '$out');
    assert.ok(result.geometry, 'Fused result should have a single flat geometry');

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
    console.log(`[Test Snap Corner] Fused Min Z: ${minZ}, Fused Max Z: ${maxZ}`);
    
    const height = maxZ - minZ;
    console.log(`[Test Snap Corner] Fused Total Height: ${height}`);
    assert.ok(height > 15.9 && height < 16.1, `Total height of the snapped assembly should be exactly 16.0 meters`);

    // 6. Capture PNG snapshot of the corner staircase
    console.log('[Test] Capturing visual rendering to actual/snap_staircase_corner.png...');
    await captureOutputPNG(results, 'snap_staircase_corner.png', { ax: 0.61547, ay: 0.78539 }, '$out');
    console.log('✔ Staircase corner snapped and saved to integration/actual/snap_staircase_corner.png');
});
