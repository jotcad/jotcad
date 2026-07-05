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

async function consumeJSON(stream) {
    const bytes = await consumeBinary(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

async function consumeText(stream) {
    const bytes = await consumeBinary(stream);
    return new TextDecoder().decode(bytes);
}

test('DXF File Import, Parsing, and Export Roundtrip', async (t) => {
    console.log("[Test] Launching cluster for DXF integration test...");
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

    const vfs = new VFS({ id: 'test-dxf-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    registerFileProvider(vfs, mesh);

    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();

    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    compiler.registerOperator('Dxf', {
        path: 'jot/Dxf',
        schema: {
            arguments: [{ name: 'file', type: 'jot:file' }],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('dxf', {
        path: 'jot/dxf',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [{ name: 'path', type: 'jot:string', default: 'export.dxf' }],
            outputs: {
                "$out": { type: 'jot:shape' },
                "file": { type: 'file', mimeType: 'image/vnd.dxf' }
            }
        }
    });

    // Create a comprehensive test DXF file containing:
    // - LINE entity
    // - CIRCLE entity
    // - ARC entity
    // - LWPOLYLINE (closed) entity
    const dxfContent = `  0
SECTION
  2
ENTITIES
  0
LINE
  8
0
 10
0.0
 20
0.0
 30
0.0
 11
10.0
 21
0.0
 31
0.0
  0
CIRCLE
  8
0
 10
50.0
 20
50.0
 30
0.0
 40
10.0
  0
ARC
  8
0
 10
100.0
 20
100.0
 30
0.0
 40
20.0
 50
45.0
 51
135.0
  0
LWPOLYLINE
  8
0
 90
3
 70
1
 10
200.0
 20
200.0
 10
250.0
 20
200.0
 10
225.0
 20
250.0
  0
ENDSEC
  0
EOF
`;

    const testDxfPath = path.join(__dirname, 'test_input.dxf');
    fs.writeFileSync(testDxfPath, dxfContent);

    try {
        // --- Test 1: DXF Import ---
        const importCode = `Dxf(File("integration/test_input.dxf")) -> $out`;
        const importAst = parser.parse(importCode);
        
        console.log("[Test] Evaluating DXF Import...");
        const importTerminals = await compiler.evaluate(importAst, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const importBundle = importTerminals.find(t => t.selector.output === '$out');
        assert.ok(importBundle, "Should find terminal output bundle");

        const shapeResult = await vfs.readSelector(importBundle.selector);
        assert.ok(shapeResult && shapeResult.stream, "Should find shape in VFS");
        
        const shapeObj = await consumeJSON(shapeResult.stream);
        console.log("[Test] Imported Shape Object:", JSON.stringify(shapeObj, null, 2));

        assert.strictEqual(shapeObj.tags.type, "wire", "Imported shape should be of type wire");
        assert.ok(shapeObj.geometry, "Imported shape must reference a geometry CID");

        // Read the geometry text to verify discretization
        const geoRes = await vfs.readCID(shapeObj.geometry);
        const geoText = await consumeText(geoRes.stream);
        console.log("[Test] Imported Geometry text snippet:\n", geoText.substring(0, 300));

        // Parse header lines
        const lines = geoText.split("\n");
        const vCountLine = lines.find(l => l.startsWith("V "));
        const sCountLine = lines.find(l => l.startsWith("S "));
        const vCount = parseInt(vCountLine.split(" ")[1]);
        const sCount = parseInt(sCountLine.split(" ")[1]);
        
        console.log(`[Test] Vertices parsed: ${vCount}, Segments parsed: ${sCount}`);
        
        // Assertions:
        // - LINE: 2 vertices, 1 segment
        // - CIRCLE: 64 segments, 64 vertices
        // - ARC: 19 vertices, 18 segments
        // - LWPOLYLINE: 3 vertices, 3 segments (closed)
        assert.ok(vCount > 70, "Should have more than 70 vertices due to curve discretization");
        assert.ok(sCount > 70, "Should have more than 70 segments due to curve discretization");

        // --- Test 2: DXF Export Roundtrip ---
        const exportCode = `$in.dxf(path="export_roundtrip.dxf").file -> file`;
        const exportAst = parser.parse(exportCode);
        
        console.log("[Test] Evaluating DXF Export...");
        const exportTerminals = await compiler.evaluate(exportAst, {
            '$in': importBundle.selector
        }, {
            outputs: {
                "file": { type: "file" }
            }
        });

        const fileBundle = exportTerminals.find(t => t.selector.output === 'file');
        assert.ok(fileBundle, "Should find file output bundle");

        const dxfExportRes = await vfs.readSelector(fileBundle.selector);
        assert.ok(dxfExportRes && dxfExportRes.stream, "Should find exported DXF in VFS");
        
        const exportedText = await consumeText(dxfExportRes.stream);
        console.log("[Test] Exported DXF Snippet:\n", exportedText.substring(0, 400));

        // Assert that the output contains standard DXF components
        assert.ok(exportedText.includes("SECTION"), "Exported DXF should contain SECTION");
        assert.ok(exportedText.includes("ENTITIES"), "Exported DXF should contain ENTITIES");
        assert.ok(exportedText.includes("LINE"), "Exported DXF should contain LINE entities");
        assert.ok(exportedText.includes("EOF"), "Exported DXF should end with EOF");

        console.log("✔ DXF Import, parsing, and export roundtrip verified successfully.");

    } finally {
        if (fs.existsSync(testDxfPath)) {
            fs.unlinkSync(testDxfPath);
        }
        await mesh.stop();
        await sys.stop();
    }
});
