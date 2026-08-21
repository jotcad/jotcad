import assert from 'node:assert';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Snap Tetrahedron Face-to-Face Integration Test', async ({
    compiler,
    parser,
    readData,
    readOutput,
    captureOutputPNG
}) => {
    const script = `
    tet1 = Tetrahedron(20).color('gold')
    tet2 = Tetrahedron(20).color('cornflowerblue')
    
    // Extract side face 1 from tet1 and tet2
    f1 = tet1.faces().nth(1)
    f2 = tet2.faces().nth(1)
    
    // Snap tet2 onto tet1
    snapped = tet2.snap(tet2.faces().nth(1), faces().nth(1), tet1)
    
    tet1 -> $tet1
    tet2 -> $tet2
    f1 -> $f1
    snapped -> $snapped
    `.trim();

    console.log('[Test] Evaluating JOT script for tetrahedron snap...');
    const ast = parser.parse(script);
    const results = await compiler.evaluate(ast, {}, {
        outputs: {
            $tet1: { type: 'jot:shape' },
            $tet2: { type: 'jot:shape' },
            $f1: { type: 'jot:shape' },
            $snapped: { type: 'jot:shape' }
        }
    });

    const tet1Shape = await readOutput(results, '$tet1');
    const f1Shape = await readOutput(results, '$f1');
    const snappedShape = await readOutput(results, '$snapped');

    console.log('[Test] tet1 tags:', JSON.stringify(tet1Shape.tags));
    console.log('[Test] f1 transform matrix:', JSON.stringify(f1Shape.tf));
    console.log('[Test] snapped components count:', snappedShape.components ? snappedShape.components.length : 1);

    if (snappedShape.components && snappedShape.components.length >= 2) {
        console.log('[Test] Comp 0 (snapped tet2) tf:', JSON.stringify(snappedShape.components[0].tf));
        console.log('[Test] Comp 1 (target tet1) tf:', JSON.stringify(snappedShape.components[1].tf));
    }

    assert.ok(snappedShape, 'Expected snapped shape to be defined');
    console.log('✅ Snap Tetrahedron Integration Test passed AST and execution.');
});
