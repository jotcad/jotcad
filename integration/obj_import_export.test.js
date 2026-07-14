import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import fs from 'node:fs';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

runIntegrationTest('OBJ Import and Export End-to-End Integration', async ({ vfs, mesh, compiler, parser, readData }) => {
    // Register File provider
    registerFileProvider(vfs, mesh);

    // Register our File operator since it is client-side
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
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

        const objBytes = await readData(objBundle.selector);
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

        const shapeData = await readData(shapeBundle.selector);
        const shapeObj = (typeof shapeData === 'object' && !(shapeData instanceof Uint8Array))
            ? shapeData
            : JSON.parse(typeof shapeData === 'string' ? shapeData : new TextDecoder().decode(shapeData));
        console.log("Imported Shape JSON:", JSON.stringify(shapeObj, null, 2));

        assert.strictEqual(shapeObj.tags.type, 'closed', 'Imported shape should be marked as closed');
        assert.ok(shapeObj.geometry, 'Imported shape should have a valid geometry CID');

        console.log("SUCCESS: OBJ Import/Export round-trip test passed!");

    } finally {
        console.log("Cleaning up test files...");
        if (fs.existsSync(testFile)) {
            fs.unlinkSync(testFile);
        }
    }
});
