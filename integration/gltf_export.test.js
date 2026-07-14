import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runIntegrationTest } from './harness.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

runIntegrationTest('glTF/GLB Binary File Export Integration', async ({ vfs, mesh, compiler, parser, evaluate, readOutput }) => {
    registerFileProvider(vfs, mesh);

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
        console.log("[Test] Evaluating SVG Import...");
        const importTerminals = await evaluate(`Svg(File("integration/test_input.svg")) -> $out`);
        
        const importBundle = importTerminals.find(t => t.port === '$out');
        assert.ok(importBundle, "Should find terminal output bundle");

        // 2. Evaluate glTF/GLB Export: $shape.gltf(path="export.glb").file -> file
        console.log("[Test] Evaluating glTF/GLB Export...");
        const exportTerminals = await compiler.evaluate(parser.parse(`$in.gltf(path="export.glb").file -> file`), {
            '$in': importBundle.selector
        }, {
            outputs: {
                "file": { type: "file" }
            }
        });

        const glbBytes = await readOutput(exportTerminals, 'file');
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
        const meshObj = gltf.meshes[0];
        assert.ok(meshObj.primitives && meshObj.primitives.length > 0, "Mesh should have primitives");
        
        const prim = meshObj.primitives[0];
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
    }
});
