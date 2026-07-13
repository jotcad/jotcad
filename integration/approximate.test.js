import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { getCID, Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceDir = path.resolve(__dirname, '..');
const libDir = path.resolve(workspaceDir, 'lib');

function parseGeo(geoText) {
    const lines = geoText.trim().split('\n');
    let numVertices = 0;
    let numTriangles = 0;
    let numFaces = 0;
    let numSegments = 0;
    const triangles = [];
    
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line.startsWith('V ')) {
            numVertices = parseInt(line.slice(2).trim(), 10);
        } else if (line.startsWith('T ')) {
            numTriangles = parseInt(line.slice(2).trim(), 10);
            for (let j = 1; j <= numTriangles; j++) {
                if (i + j >= lines.length) break;
                const parts = lines[i + j].trim().split(/\s+/).map(Number);
                triangles.push(parts);
            }
            i += numTriangles;
        } else if (line.startsWith('F ')) {
            numFaces = parseInt(line.slice(2).trim(), 10);
            i += numFaces;
        } else if (line.startsWith('S ')) {
            numSegments = parseInt(line.slice(2).trim(), 10);
            i += numSegments;
        }
    }
    
    const edgeMap = new Map();
    for (const t of triangles) {
        if (t.length < 3) continue;
        const edges = [
            [t[0], t[1]],
            [t[1], t[2]],
            [t[2], t[0]]
        ];
        for (const edge of edges) {
            const eKey = edge[0] < edge[1] ? `${edge[0]}-${edge[1]}` : `${edge[1]}-${edge[0]}`;
            edgeMap.set(eKey, (edgeMap.get(eKey) || 0) + 1);
        }
    }
    
    let boundaryEdges = 0;
    let nonManifoldEdges = 0;
    for (const count of edgeMap.values()) {
        if (count === 1) boundaryEdges++;
        else if (count > 2) nonManifoldEdges++;
    }
    
    return {
        vertices: numVertices,
        triangles: numTriangles,
        faces: numFaces,
        segments: numSegments,
        boundaryEdges,
        nonManifoldEdges
    };
}

runIntegrationTest('VSA Mesh Approximation Integration Test', async ({ vfs, readData, capturePNG }) => {
    const geoPath = path.resolve(libDir, '28byj-48_5vdc.geo');
    const shapePath = path.resolve(libDir, '28byj-48_5vdc.json');

    assert.ok(fs.existsSync(geoPath), "Original geometry file must exist");
    assert.ok(fs.existsSync(shapePath), "Original shape JSON must exist");

    const geometryText = fs.readFileSync(geoPath, 'utf8');
    const shapeJSONText = fs.readFileSync(shapePath, 'utf8');

    const geoCID = await getCID(geometryText);
    const shapeCID = await getCID(shapeJSONText);

    await vfs.write(geoCID, geometryText, { encoding: 'string', filename: '28byj-48_5vdc.geo' });
    await vfs.write(shapeCID, shapeJSONText, { encoding: 'json', filename: '28byj-48_5vdc.json' });

    // Stage 1: jot/wrap
    console.log("[Test] Querying wrap operator...");
    const wrapSel = new Selector('jot/wrap', {
        $in: shapeCID,
        alpha: 0.4,
        offset: 0.05
    }).withOutput('$out');

    const wrappedShape = await readData(wrapSel);
    assert.ok(wrappedShape, "Wrap operator must return a valid VFS entry");
    assert.ok(wrappedShape.geometry, "Wrapped shape must carry a geometry CID reference");

    const wrappedGeoText = await readData(wrappedShape.geometry);

    // Write wrapped shape/geometry to client VFS for downstream approximate step
    const wrappedShapeText = JSON.stringify(wrappedShape);
    const wrappedShapeCID = await getCID(wrappedShapeText);
    await vfs.write(wrappedShapeCID, wrappedShapeText, { encoding: 'json', filename: '28byj-48_5vdc_wrapped.json' });
    await vfs.write(wrappedShape.geometry, wrappedGeoText, { encoding: 'string', filename: '28byj-48_5vdc_wrapped.geo' });

    // Stage 2: jot/approximate
    const maxProxies = 120;
    const approxSel = new Selector('jot/approximate', {
        $in: wrappedShapeCID,
        max_proxies: maxProxies
    }).withOutput('$out');

    console.log("[Test] Querying approximate operator...");
    const approxShape = await readData(approxSel);

    assert.ok(approxShape, "Approximate operator must return a valid VFS entry");
    assert.ok(approxShape.geometry, "Approximated shape must carry a geometry CID reference");

    console.log("[Test] Fetching approximated geometry...");
    const approxGeoText = await readData(approxShape.geometry);

    const originalStats = parseGeo(geometryText);
    const wrappedStats = parseGeo(wrappedGeoText);
    const approxStats = parseGeo(approxGeoText);

    console.log(`[Test] Original Triangles: ${originalStats.triangles}, Vertices: ${originalStats.vertices}`);
    console.log(`[Test] Wrapped Triangles: ${wrappedStats.triangles}, Vertices: ${wrappedStats.vertices}`);
    console.log(`[Test] Approximated Triangles: ${approxStats.triangles}, Vertices: ${approxStats.vertices}`);

    // Topological Assertions
    assert.strictEqual(wrappedStats.nonManifoldEdges, 0, "Wrapped shape must have 0 non-manifold edges");
    assert.ok(approxStats.triangles < wrappedStats.triangles, "Triangle count must be reduced from wrap");
    assert.ok(approxStats.vertices < wrappedStats.vertices, "Vertex count must be reduced from wrap");
    assert.strictEqual(approxStats.nonManifoldEdges, 0, "Approximated shape must have 0 non-manifold edges");

    // Write approximated shape to client VFS for rendering
    const approxShapeText = JSON.stringify(approxShape);
    const approxShapeCID = await getCID(approxShapeText);
    await vfs.write(approxShapeCID, approxShapeText, { encoding: 'json', filename: '28byj-48_5vdc_approx.json' });
    await vfs.write(approxShape.geometry, approxGeoText, { encoding: 'string', filename: '28byj-48_5vdc_approx.geo' });

    // Verify PNG rendering is functional and save images for reference
    console.log("[Test] Rendering and saving wrapped shape PNG...");
    await capturePNG(wrappedShapeCID, '28byj-48_5vdc_wrapped.png', {
        width: 512,
        height: 512,
        ax: 45,
        ay: 45
    });

    console.log("[Test] Rendering and saving approximated shape PNG...");
    await capturePNG(approxShapeCID, '28byj-48_5vdc_approx.png', {
        width: 512,
        height: 512,
        ax: 45,
        ay: 45
    });
});
