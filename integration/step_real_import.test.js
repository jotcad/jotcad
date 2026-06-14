import test from 'node:test';
import assert from 'node:assert';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS, MeshLink } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const stepFilePath = path.resolve(__dirname, 'esp32-wemos-d1-mini.step');

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

async function consumeJSON(stream) {
    const bytes = await consumeBinary(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('Real STEP File Import and Render Integration', async (t) => {
    console.log("[Test] Launching cluster for real STEP integration test...");
    
    // 1. Launch the TEST system profile
    const sys = await launchSystem('test/standard');
    const exportPort = sys.ports.export;

    // 2. Setup client VFS and MeshLink connected to the export node
    const vfs = new VFS({ id: 'test-real-step-client' });
    const mesh = new MeshLink(vfs, [`https://localhost:${exportPort}`]);
    
    await vfs.init();
    await mesh.start();

    // Register File provider on client VFS
    registerFileProvider(vfs, mesh);

    // 3. Setup Jot Compiler & Parser
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();

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

    try {
        console.log(`[Test] Importing STEP file from: ${stepFilePath}`);
        const importCode = `Step(File("${stepFilePath.replace(/\\/g, '/')}")) -> $out`;
        
        console.log(`[Test] Evaluating import DSL: ${importCode}`);
        const importAst = parser.parse(importCode);
        const importTerminals = await compiler.evaluate(importAst, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const importBundle = importTerminals.find(t => t.selector.output === '$out');
        assert.ok(importBundle, "Should find terminal output bundle");
        
        console.log("[Test] Fetching shape from VFS via readSelector...");
        const shapeResult = await vfs.readSelector(importBundle.selector);
        assert.ok(shapeResult, "readSelector should find shape entry");
        
        const shape = await consumeJSON(shapeResult.stream);
        assert.ok(shape, "VFS should return a valid shape object");
        assert.ok(shape.geometry, "Shape must carry a geometry CID reference");
        
        console.log(`[Test] Shape loaded. Name: ${shape.tags.name}, Type: ${shape.tags.type}`);
        assert.strictEqual(shape.tags.name, "esp32-wemos-d1-mini.step", "Shape name tag must match the file name");

        // Render for visual confirmation
        console.log("[Test] Rendering shape to PNG...");
        const pngBytes = await captureAndVerifyPNG(vfs, importBundle.selector, 'esp32-wemos-d1-mini_result.png', null, { ax: 0.61547, ay: 0.78539 });
        assert.ok(pngBytes.length > 0, "Rendered PNG should not be empty");
        console.log(`✔ STEP Real Import and Render completed successfully.`);
        
    } finally {
        console.log("[Test] Cleaning up...");
        mesh.stop();
        await vfs.close();
        await sys.stop();
    }
});
