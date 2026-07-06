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

test('STL Import and Export End-to-End Integration', async (t) => {
    console.log("Starting STL Import/Export Round-Trip Test...");
    
    // 1. Launch system
    const sys = await launchSystem('test/standard');
    const vfs = new VFS({ id: 'test-stl-client' });
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

    compiler.registerOperator('stl', {
        path: 'jot/stl',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'path', type: 'jot:string', default: 'export.stl' }
            ],
            outputs: {
                "$out": { type: 'file' }
            }
        }
    });

    compiler.registerOperator('Stl', {
        path: 'jot/Stl',
        schema: {
            arguments: [
                { name: 'file', type: 'jot:file' }
            ],
            outputs: {
                "$out": { type: 'jot:shape' }
            }
        }
    });

    const testFile = 'imported.stl';

    try {
        // --- STEP 1: Export a Box to STL ---
        const exportCode = 'Box(10).stl("box_export.stl") -> $out';
        console.log("[Test] Evaluating export DSL:", exportCode);
        const exportAst = parser.parse(exportCode);
        const exportTerminals = await compiler.evaluate(exportAst, {}, {
            outputs: { "$out": { type: "file" } }
        });
        
        const stlBundle = exportTerminals.find(t => t.selector.output === '$out');
        assert.ok(stlBundle, "Should find export terminal");

        const result = await vfs.read(stlBundle.selector);
        assert.ok(result, "Should read STL file from VFS");
        const stlBytes = await consumeBinary(result.stream);
        console.log(`[Test] Exported STL file size: ${stlBytes.byteLength} bytes.`);
        assert.ok(stlBytes.byteLength > 84, "Exported STL should have a valid header + count");

        // --- STEP 2: Write STL bytes back to disk file ---
        fs.writeFileSync(testFile, Buffer.from(stlBytes));
        console.log(`[Test] Wrote STL bytes to disk: "${testFile}"`);

        // --- STEP 3: Import it back using Stl(File("imported.stl")) ---
        const importCode = `Stl(File("${testFile}")) -> $out`;
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

        console.log("SUCCESS: STL Import/Export round-trip test passed!");

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
