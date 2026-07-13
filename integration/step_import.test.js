import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

runIntegrationTest('STEP File Import and Export Integration', async ({ vfs, mesh, compiler, parser, readData }) => {
    // Register File provider on client VFS
    registerFileProvider(vfs, mesh);

    // Register our File constructor since it is a client-side provider
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    // --- Test Step 1: Import a shape ---
    const importCode = 'Step(File("test_model.stp")) -> $out';
    console.log(`[Test] Evaluating import DSL: ${importCode}`);
    
    const importAst = parser.parse(importCode);
    const importTerminals = await compiler.evaluate(importAst, {}, {
        outputs: { "$out": { type: "jot:shape" } }
    });
    
    const importBundle = importTerminals.find(t => t.selector.output === '$out');
    assert.ok(importBundle, "Should find terminal output bundle");
    
    console.log("[Test] Fetching shape from VFS...");
    const shapeData = await readData(importBundle.selector);
    const shape = (typeof shapeData === 'object' && !(shapeData instanceof Uint8Array))
        ? shapeData
        : JSON.parse(typeof shapeData === 'string' ? shapeData : new TextDecoder().decode(shapeData));
    assert.ok(shape, "VFS should return a valid shape object");
    assert.ok(shape.geometry, "Shape must carry a geometry CID reference");
    assert.strictEqual(shape.tags.type, "surface", "Shape type tag must match");
    assert.strictEqual(shape.tags.name, "mock_triangle", "Shape name tag must match");
    
    console.log("[Test] Fetching raw geometry text from VFS via readData...");
    const geometryData = await readData(shape.geometry);
    const geometryText = (typeof geometryData === 'string')
        ? geometryData
        : new TextDecoder().decode(geometryData);
    assert.ok(geometryText.includes("V 3"), "Geometry should define 3 vertices");
    assert.ok(geometryText.includes("F 1"), "Geometry should define 1 face");
    console.log("✔ STEP Import evaluated successfully.");

    // --- Test Step 2: Export a shape ---
    const exportCode = 'Step(File("test_model.stp")).step("out_mesh.stp").file -> file';
    console.log(`[Test] Evaluating export DSL: ${exportCode}`);
    
    const exportAst = parser.parse(exportCode);
    const exportTerminals = await compiler.evaluate(exportAst, {}, {
        outputs: { "file": { type: "file" } }
    });
    
    const exportBundle = exportTerminals.find(t => t.selector.output === 'file');
    assert.ok(exportBundle, "Should find 'file' output terminal bundle");
    
    console.log("[Test] Fetching exported file stream from VFS...");
    const fileData = await readData(exportBundle.selector);
    const fileText = (typeof fileData === 'string')
        ? fileData
        : new TextDecoder().decode(fileData);
    
    assert.ok(fileText.startsWith("ISO-10303-21;"), "Exported file must start with STEP header");
    assert.ok(fileText.includes("FILE_NAME('out_mesh.stp'"), "Exported file must carry the correct filename");
    console.log("✔ STEP Export evaluated successfully.");
});
