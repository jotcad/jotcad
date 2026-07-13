import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import fs from 'node:fs';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

runIntegrationTest('STL Import and Export End-to-End Integration', async ({ vfs, mesh, compiler, parser, readData }) => {
    // Register File provider
    registerFileProvider(vfs, mesh);

    // Register our File constructor since it is a client-side provider
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
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

        const stlBytes = await readData(stlBundle.selector);
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

        const shapeData = await readData(shapeBundle.selector);
        const shapeObj = (typeof shapeData === 'object' && !(shapeData instanceof Uint8Array))
            ? shapeData
            : JSON.parse(typeof shapeData === 'string' ? shapeData : new TextDecoder().decode(shapeData));
        console.log("Imported Shape JSON:", JSON.stringify(shapeObj, null, 2));

        assert.strictEqual(shapeObj.tags.type, 'closed', 'Imported shape should be marked as closed');
        assert.ok(shapeObj.geometry, 'Imported shape should have a valid geometry CID');

        console.log("SUCCESS: STL Import/Export round-trip test passed!");

    } finally {
        console.log("Cleaning up test files...");
        if (fs.existsSync(testFile)) {
            fs.unlinkSync(testFile);
        }
    }
});
