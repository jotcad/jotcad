import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { runIntegrationTest } from './harness.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

runIntegrationTest('SVG File Import Integration', async ({ vfs, mesh, compiler, parser, evaluate, readData }) => {
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
        console.log("[Test] Evaluating SVG Import expression...");
        const terminals = await evaluate(`Svg(File("integration/test_donut.svg")) -> $out`);
        
        const result = terminals.find(t => t.port === '$out');
        assert.ok(result, "Evaluation result should contain $out");

        // Resolve shape structure
        const shapeObj = await readData(result.selector);
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
        const geoText = await readData(pathComp.geometry);
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
    }
});
