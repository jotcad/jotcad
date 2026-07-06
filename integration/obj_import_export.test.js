import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import { VFS, MeshLink } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

async function consumeBinary(stream) {
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

test('OBJ Import and Export End-to-End Integration', async (t) => {
    console.log("Starting OBJ Import/Export Round-Trip Test...");
    
    // 1. Launch system
    const sys = await launchSystem('test/standard');
    const vfs = new VFS({ id: 'test-obj-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    // Register File provider
    registerFileProvider(vfs, mesh);

    // 2. Setup Jot Compiler
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();
    
    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'width', type: 'jot:interval' }],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    compiler.registerOperator('obj', {
        path: 'jot/obj',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'path', type: 'jot:string', default: 'export.obj' }
            ],
            outputs: {
                "$out": { type: 'file' }
            }
        }
    });

    compiler.registerOperator('Obj', {
        path: 'jot/Obj',
        schema: {
            arguments: [
                { name: 'file', type: 'jot:file' }
            ],
            outputs: {
                "$out": { type: 'jot:shape' }
            }
        }
    });

    const testFile = 'imported.obj';

    try {
        // --- STEP 1: Export a Box to OBJ ---
        const exportCode = 'Box(10).obj("box_export.obj") -> $out';
        console.log("[Test] Evaluating export DSL:", exportCode);
        const exportAst = parser.parse(exportCode);
        const exportTerminals = await compiler.evaluate(exportAst, {}, {
            outputs: { "$out": { type: "file" } }
        });
        
        const objBundle = exportTerminals.find(t => t.selector.output === '$out');
        assert.ok(objBundle, "Should find export terminal");

        const result = await vfs.read(objBundle.selector);
        assert.ok(result, "Should read OBJ file from VFS");
        const objBytes = await consumeBinary(result.stream);
        const objText = new TextDecoder().decode(objBytes);
        console.log(`[Test] Exported OBJ text:\n${objText}`);
        assert.ok(objText.includes('v '), "Exported OBJ should contain vertices");
        assert.ok(objText.includes('f '), "Exported OBJ should contain faces");

        // --- STEP 2: Write OBJ bytes back to disk file ---
        fs.writeFileSync(testFile, Buffer.from(objBytes));
        console.log(`[Test] Wrote OBJ text to disk: "${testFile}"`);

        // --- STEP 3: Import it back using Obj(File("imported.obj")) ---
        const importCode = `Obj(File("${testFile}")) -> $out`;
        console.log("[Test] Evaluating import DSL:", importCode);
        const importAst = parser.parse(importCode);
        const importTerminals = await compiler.evaluate(importAst, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });

        const shapeBundle = importTerminals.find(t => t.selector.output === '$out');
        assert.ok(shapeBundle, "Should find import shape terminal");

        const shapeResult = await vfs.read(shapeBundle.selector);
        assert.ok(shapeResult, "Should read imported shape from VFS");
        
        const shapeObj = JSON.parse(new TextDecoder().decode(await consumeBinary(shapeResult.stream)));
        console.log("Imported Shape JSON:", JSON.stringify(shapeObj, null, 2));

        assert.strictEqual(shapeObj.tags.type, 'closed', 'Imported shape should be marked as closed');
        assert.ok(shapeObj.geometry, 'Imported shape should have a valid geometry CID');

        console.log("SUCCESS: OBJ Import/Export round-trip test passed!");

    } finally {
        console.log("Cleaning up test files...");
        if (fs.existsSync(testFile)) {
            fs.unlinkSync(testFile);
        }
        await mesh.stop();
        await vfs.close();
        await sys.stop();
        setTimeout(() => process.exit(0), 500);
    }
});
