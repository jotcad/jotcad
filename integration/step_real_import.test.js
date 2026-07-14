import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runIntegrationTest } from './harness.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const stepFilePath = path.resolve(__dirname, 'esp32-wemos-d1-mini.step');

runIntegrationTest('Real STEP File Import and Render Integration', async ({ vfs, mesh, compiler, parser, evaluate, readData, capturePNG }) => {
    // Register File provider on client VFS
    registerFileProvider(vfs, mesh);

    // Register our File constructor
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [
                { name: 'path', type: 'jot:string' }
            ],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    // Register our Step constructor
    compiler.registerOperator('Step', {
        path: 'jot/Step',
        schema: {
            arguments: [
                { name: 'file', type: 'jot:file' },
                { name: 'deflection', type: 'jot:number', default: 0.1 }
            ],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    console.log(`[Test] Importing STEP file from: ${stepFilePath}`);
    const importCode = `Step(File("${stepFilePath.replace(/\\/g, '/')}")) -> $out`;
    
    console.log(`[Test] Evaluating import DSL: ${importCode}`);
    const importTerminals = await evaluate(importCode);
    
    const importBundle = importTerminals.find(t => t.port === '$out');
    assert.ok(importBundle, "Should find terminal output bundle");
    
    console.log("[Test] Fetching shape from VFS via readSelector...");
    const shape = await readData(importBundle.selector);
    assert.ok(shape, "VFS should return a valid shape object");
    assert.ok(shape.geometry, "Shape must carry a geometry CID reference");
    
    console.log(`[Test] Shape loaded. Name: ${shape.tags.name}, Type: ${shape.tags.type}`);
    assert.strictEqual(shape.tags.name, "esp32-wemos-d1-mini.step", "Shape name tag must match the file name");

    // Render for visual confirmation
    console.log("[Test] Rendering shape to PNG...");
    const pngBytes = await capturePNG(importBundle.selector, 'esp32-wemos-d1-mini_result.png', { ax: 0.61547, ay: 0.78539 });
    assert.ok(pngBytes.length > 0, "Rendered PNG should not be empty");
    console.log(`✔ STEP Real Import and Render completed successfully.`);
});
