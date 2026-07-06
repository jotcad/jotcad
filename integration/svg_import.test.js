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

// Disable TLS verification for self-signed certificates in local tests
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

async function consumeJSON(stream) {
    const bytes = await consumeBinary(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

test('SVG File Import Integration', async (t) => {
    console.log("[Test] Launching cluster for SVG integration test...");
    
    // 1. Launch the TEST system profile
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

    // 2. Setup client VFS and MeshLink connected to the router
    const vfs = new VFS({ id: 'test-svg-client' });
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

    // Register our Svg constructor
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

    // Create a mock SVG file with:
    // - One rectangle (first disjoint surface)
    // - One donut shape (square outer boundary + nested square hole = second disjoint surface)
    const testSvgPath = path.join(__dirname, 'test_donut.svg');
    const svgContent = `
<svg width="200" height="200" xmlns="http://www.w3.org/2000/svg">
  <!-- Sibling rectangle -->
  <rect x="10" y="20" width="100" height="50" />
  
  <!-- Donut boundary with nested hole -->
  <path d="M 0,0 L 50,0 L 50,50 L 0,50 Z M 10,10 L 10,40 L 40,40 L 40,10 Z" />
</svg>
`;
    fs.writeFileSync(testSvgPath, svgContent);

    try {
        // Evaluate: Svg(File("integration/test_donut.svg")) -> $out
        const code = `Svg(File("integration/test_donut.svg")) -> $out`;
        const ast = parser.parse(code);
        
        console.log("[Test] Evaluating SVG Import expression...");
        const terminals = await compiler.evaluate(ast, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const result = terminals.find(t => t.selector.output === '$out');
        assert.ok(result, "Evaluation result should contain $out");

        // Resolve shape structure
        const shapeResult = await vfs.readSelector(result.selector);
        assert.ok(shapeResult && shapeResult.stream, "Should find shape in VFS");
        const shapeObj = await consumeJSON(shapeResult.stream);
        console.log("[Test] Evaluated Shape Structure:", JSON.stringify(shapeObj, null, 2));

        // Assertions
        assert.strictEqual(shapeObj.tags.type, "group", "Root shape should be a Group");
        assert.ok(Array.isArray(shapeObj.components), "Group should have components list");
        
        // We expect exactly 2 components (the rectangle and the path-with-hole)
        assert.strictEqual(shapeObj.components.length, 2, "Should parse exactly 2 disjoint surface components");

        // Verify shape properties
        const rectComp = shapeObj.components[0];
        assert.strictEqual(rectComp.tags.type, "surface", "First component should be a surface");
        assert.ok(rectComp.geometry, "First component should reference a geometry CID");

        const pathComp = shapeObj.components[1];
        assert.strictEqual(pathComp.tags.type, "surface", "Second component should be a surface");
        assert.ok(pathComp.geometry, "Second component should reference a geometry CID");

        // Read the actual geometry representation of the path with a hole from VFS
        const geoRes = await vfs.readCID(pathComp.geometry);
        const geoText = await consumeText(geoRes.stream);
        console.log("[Test] Triangulated Geometry Text for donut:", geoText);

        // Verify triangles and vertices exist in the triangulated output
        assert.ok(geoText.startsWith("V "), "Geometry text must start with Vertices header");
        assert.ok(geoText.includes("T "), "Geometry text must contain Triangles header");
        assert.ok(geoText.includes("S "), "Geometry text must contain Segments header");

        console.log("✔ SVG Import evaluated and verified successfully.");

    } finally {
        // Clean up test SVG file
        if (fs.existsSync(testSvgPath)) {
            fs.unlinkSync(testSvgPath);
        }
        // Shutdown mesh and system
        await mesh.stop();
        await sys.stop();
    }
});

async function consumeText(stream) {
    const bytes = await consumeBinary(stream);
    return new TextDecoder().decode(bytes);
}
