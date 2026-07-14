import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runIntegrationTest } from './harness.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

runIntegrationTest('DXF File Import, Parsing, and Export Roundtrip', async ({ vfs, mesh, compiler, parser, evaluate, readData, readOutput }) => {
    registerFileProvider(vfs, mesh);

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
        console.log("[Test] Evaluating DXF Import...");
        const importTerminals = await evaluate(`Dxf(File("integration/test_input.dxf")) -> $out`);
        
        const importBundle = importTerminals.find(t => t.port === '$out');
        assert.ok(importBundle, "Should find terminal output bundle");

        const shapeObj = await readData(importBundle.selector);
        assert.ok(shapeObj, "Should find shape in VFS");
        console.log("[Test] Imported Shape Object:", JSON.stringify(shapeObj, null, 2));

        assert.strictEqual(shapeObj.tags.type, "wire", "Imported shape should be of type wire");
        assert.ok(shapeObj.geometry, "Imported shape must reference a geometry CID");

        // Read the geometry text to verify discretization
        const geoText = await readData(shapeObj.geometry);
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
        console.log("[Test] Evaluating DXF Export...");
        const exportTerminals = await compiler.evaluate(parser.parse(`$in.dxf(path="export_roundtrip.dxf").file -> file`), {
            '$in': importBundle.selector
        }, {
            outputs: {
                "file": { type: "file" }
            }
        });

        const dxfExportBytes = await readOutput(exportTerminals, 'file');
        const exportedText = new TextDecoder().decode(dxfExportBytes);
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
    }
});
