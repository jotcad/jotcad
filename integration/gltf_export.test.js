import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS, MeshLink } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

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

test('glTF/GLB Binary File Export Integration', async (t) => {
    console.log("[Test] Launching cluster for glTF integration test...");
    const sys = await launchSystem('test/standard');

    // Wait for Export Node (port 9202) to be fully ready
    console.log("[Test] Waiting for Export Node to start...");
    let exportReady = false;
    for (let i = 0; i < 50; i++) {
        try {
            const resp = await fetch('https://localhost:9202', {
                dispatcher: new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } })
            });
            if (resp.ok || resp.status === 404 || resp.status === 400) {
                exportReady = true;
                break;
            }
        } catch (e) {}
        await new Promise(r => setTimeout(r, 200));
    }
    if (!exportReady) {
        console.warn("[Test] Export Node port 9202 not responding after 10s. Continuing anyway...");
    } else {
        console.log("[Test] Export Node is ready.");
    }

    const vfs = new VFS({ id: 'test-gltf-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    registerFileProvider(vfs, mesh);

    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();

    // Register operators
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    compiler.registerOperator('Svg', {
        path: 'jot/Svg',
        schema: {
            arguments: [
                { name: 'file', type: 'jot:file' },
                { name: 'deflection', type: 'jot:number', default: 0.1 }
            ],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('gltf', {
        path: 'jot/gltf',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [{ name: 'path', type: 'jot:string', default: 'export.glb' }],
            outputs: {
                "$out": { type: 'jot:shape' },
                "file": { type: 'file', mimeType: 'model/gltf-binary' }
            }
        }
    });

    // Create a mock SVG file as input geometry
    const testSvgPath = path.join(__dirname, 'test_input.svg');
    const svgContent = `
<svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
  <rect x="10" y="20" width="100" height="50" />
  <path d="M 0,0 L 50,0 L 50,50 L 0,50 Z M 10,10 L 10,40 L 40,40 L 40,10 Z" />
</svg>
`;
    fs.writeFileSync(testSvgPath, svgContent);

    try {
        // 1. Evaluate: $shape = Svg(File("integration/test_input.svg"))
        const importCode = `Svg(File("integration/test_input.svg")) -> $out`;
        const importAst = parser.parse(importCode);
        
        console.log("[Test] Evaluating SVG Import...");
        const importTerminals = await compiler.evaluate(importAst, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const importBundle = importTerminals.find(t => t.selector.output === '$out');
        assert.ok(importBundle, "Should find terminal output bundle");

        // 2. Evaluate glTF/GLB Export: $shape.gltf(path="export.glb").file -> file
        const exportCode = `$in.gltf(path="export.glb").file -> file`;
        const exportAst = parser.parse(exportCode);
        
        console.log("[Test] Evaluating glTF/GLB Export...");
        const exportTerminals = await compiler.evaluate(exportAst, {
            '$in': importBundle.selector
        }, {
            outputs: {
                "file": { type: "file" }
            }
        });

        const fileBundle = exportTerminals.find(t => t.selector.output === 'file');
        assert.ok(fileBundle, "Should find file output terminal");

        const glbRes = await vfs.readSelector(fileBundle.selector);
        assert.ok(glbRes && glbRes.stream, "VFS should find glb output");
        
        const glbBytes = await consumeBinary(glbRes.stream);
        console.log(`[Test] Exported GLB size: ${glbBytes.length} bytes`);

        // 3. Verify GLB Binary Format Headers
        const view = new DataView(glbBytes.buffer, glbBytes.byteOffset, glbBytes.byteLength);
        
        const magic = view.getUint32(0, true);
        const version = view.getUint32(4, true);
        const fileLength = view.getUint32(8, true);

        // magic: 0x46546C67 = "glTF"
        assert.strictEqual(magic, 0x46546C67, "GLB magic header must be 'glTF'");
        assert.strictEqual(version, 2, "GLB version must be 2");
        assert.strictEqual(fileLength, glbBytes.length, "GLB header length must match total bytes");

        // Chunk 0: JSON
        const chunk0Length = view.getUint32(12, true);
        const chunk0Type = view.getUint32(16, true);
        assert.strictEqual(chunk0Type, 0x4E4F534A, "First chunk type must be 'JSON'");

        const jsonOffset = 20;
        const jsonBytes = glbBytes.subarray(jsonOffset, jsonOffset + chunk0Length);
        const jsonText = new TextDecoder().decode(jsonBytes);
        const gltf = JSON.parse(jsonText);
        console.log("[Test] Parsed glTF JSON structure:", JSON.stringify(gltf, null, 2));

        // glTF Structure Assertions
        assert.ok(gltf.asset && gltf.asset.version === "2.0", "glTF version should be 2.0");
        assert.ok(Array.isArray(gltf.nodes) && gltf.nodes.length > 0, "Should have nodes list");
        assert.ok(Array.isArray(gltf.meshes) && gltf.meshes.length > 0, "Should have meshes list");
        assert.ok(Array.isArray(gltf.accessors) && gltf.accessors.length > 0, "Should have accessors");
        assert.ok(Array.isArray(gltf.bufferViews) && gltf.bufferViews.length > 0, "Should have bufferViews");
        assert.ok(Array.isArray(gltf.buffers) && gltf.buffers.length > 0, "Should have buffers");

        // Verify primitives contain position and indices
        const mesh = gltf.meshes[0];
        assert.ok(mesh.primitives && mesh.primitives.length > 0, "Mesh should have primitives");
        
        const prim = mesh.primitives[0];
        assert.ok(prim.attributes && prim.attributes.POSITION !== undefined, "Primitive must have POSITION attribute");
        assert.ok(prim.indices !== undefined, "Primitive must have indices accessor");
        
        // Chunk 1: BIN
        const binHeaderOffset = jsonOffset + chunk0Length;
        const chunk1Length = view.getUint32(binHeaderOffset, true);
        const chunk1Type = view.getUint32(binHeaderOffset + 4, true);
        assert.strictEqual(chunk1Type, 0x004E4942, "Second chunk type must be 'BIN'");
        assert.strictEqual(binHeaderOffset + 8 + chunk1Length, glbBytes.length, "Total byte count must check out");

        console.log("✔ glTF/GLB Binary File Export verified successfully.");

    } finally {
        if (fs.existsSync(testSvgPath)) {
            fs.unlinkSync(testSvgPath);
        }
        await mesh.stop();
        await sys.stop();
    }
});
