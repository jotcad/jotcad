import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

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

async function consumeText(stream) {
    const bytes = await consumeBinary(stream);
    return new TextDecoder().decode(bytes);
}

test('STEP File Import and Export Integration', async (t) => {
    console.log("[Test] Launching cluster for STEP integration test...");
    
    // 1. Launch the TEST system profile
    const sys = await launchSystem('test/standard');
    const exportPort = sys.ports.export;

    // 2. Setup client VFS and MeshLink connected to the export node
    const vfs = new VFS({ id: 'test-step-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
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

    // Register our step export operation
    compiler.registerOperator('step', {
        path: 'jot/step',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'path', type: 'jot:string', default: 'export.stp' }
            ],
            outputs: {
                "$out": { type: 'jot:shape' },
                "file": { type: 'file', mimeType: 'model/step' }
            }
        }
    });

    try {
        // --- Test Step 1: Import a shape ---
        const importCode = 'Step(File("test_model.stp")) -> $out';
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
        assert.strictEqual(shape.tags.type, "surface", "Shape type tag must match");
        assert.strictEqual(shape.tags.name, "mock_triangle", "Shape name tag must match");
        
        console.log("[Test] Fetching raw geometry text from VFS via readCID...");
        const geoResult = await vfs.readCID(shape.geometry);
        assert.ok(geoResult, "readCID should find geometry entry");
        
        const geometryText = await consumeText(geoResult.stream);
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
        const fileResult = await vfs.readSelector(exportBundle.selector);
        assert.ok(fileResult, "VFS should find file output");
        
        const fileBytes = await consumeBinary(fileResult.stream);
        const fileText = new TextDecoder().decode(fileBytes);
        
        assert.ok(fileText.startsWith("ISO-10303-21;"), "Exported file must start with STEP header");
        assert.ok(fileText.includes("FILE_NAME('out_mesh.stp'"), "Exported file must carry the correct filename");
        console.log("✔ STEP Export evaluated successfully.");

    } finally {
        console.log("[Test] Cleaning up...");
        await mesh.stop();
        await vfs.close();
        await sys.stop();
    }
});
