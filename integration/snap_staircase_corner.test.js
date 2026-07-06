import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, Selector } from '../fs/src/index.js';
import { JotParser } from '../jot/src/parser.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { decodeGeometry } from '../jot/src/index.js';
import { launchSystem } from '../orchestrator.js';
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

test('Snap Staircase Corner Integration Test', { timeout: 120000 }, async (t) => {
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

        // 3. Setup compiler and parser
        const compiler = new JotCompiler(nodeVFS);
        const parser = new JotParser();

        // Dynamically register all catalog schema entries
        for (const [path, schema] of Object.entries(catalog)) {
            const name = path.split('/').pop();
            compiler.registerOperator(name, { path, schema });
        }

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

        const getShapeData = async (sel) => {
            const res = await nodeVFS.read(sel);
            const bytes = await consumeBytes(res.stream);
            return JSON.parse(new TextDecoder().decode(bytes));
        };

        const b1Shape = await getShapeData(results.find(r => r.port === '$b1').selector);
        const b2Shape = await getShapeData(results.find(r => r.port === '$b2').selector);
        const s2Shape = await getShapeData(results.find(r => r.port === '$s2').selector);
        const s3Shape = await getShapeData(results.find(r => r.port === '$s3').selector);
        const assemblyShape = await getShapeData(results.find(r => r.port === '$assembly').selector);

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
        const s2TopShape = await getShapeData(s2TopSel);
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
        const s3TopShape = await getShapeData(s3TopSel);
        console.log('--- DEBUG s3 top ---');
        console.log(JSON.stringify(s3TopShape, null, 2));

        console.log('--- DEBUG assembly ---');
        console.log(JSON.stringify(assemblyShape, null, 2));

        const resultSelector = results.find(r => r.port === '$out').selector;

        // 5. Read and verify the geometry of the fused assembly
        const response = await nodeVFS.read(resultSelector);
        assert.ok(response, 'Should return a valid response from VFS');

        const resultBytes = await consumeBytes(response.stream);
        const result = JSON.parse(new TextDecoder().decode(resultBytes));
        assert.ok(result.geometry, 'Fused result should have a single flat geometry');

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
        console.log(`[Test Snap Corner] Fused Min Z: ${minZ}, Fused Max Z: ${maxZ}`);
        
        const height = maxZ - minZ;
        console.log(`[Test Snap Corner] Fused Total Height: ${height}`);
        assert.ok(height > 15.9 && height < 16.1, `Total height of the snapped assembly should be exactly 16.0 meters`);

        // 6. Capture PNG snapshot of the corner staircase
        console.log('[Test] Capturing visual rendering to actual/snap_staircase_corner.png...');
        await captureAndVerifyPNG(nodeVFS, resultSelector, 'snap_staircase_corner.png', null, { ax: 0.61547, ay: 0.78539 });
        console.log('✔ Staircase corner snapped and saved to integration/actual/snap_staircase_corner.png');

    } finally {
        await mesh.stop();
        await nodeVFS.close();
        await sys.stop();
    }
});
