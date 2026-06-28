import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS, MeshLink, getCID, Selector } from '../fs/src/index.js';
import { launchSystem } from '../orchestrator.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceDir = path.resolve(__dirname, '..');
const libDir = path.resolve(workspaceDir, 'lib');

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

test('VSA Mesh Approximation Integration Test', async (t) => {
    console.log("[Test] Launching cluster for VSA approximate integration test...");
    const sys = await launchSystem('test/standard');

    const vfs = new VFS({ id: 'test-approximate-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    try {
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

        const maxProxies = 40;
        const approxSel = new Selector('jot/approximate', {
            $in: shapeCID,
            max_proxies: maxProxies
        }).withOutput('$out');

        console.log("[Test] Querying approximate operator...");
        const context = { expiresAt: Date.now() + 600000 };
        const result = await vfs.readSelector(approxSel, context);

        assert.ok(result, "Approximate operator must return a valid VFS entry");
        const approxShape = await consumeJSON(result.stream);
        assert.ok(approxShape.geometry, "Approximated shape must carry a geometry CID reference");

        console.log("[Test] Fetching approximated geometry...");
        const approxGeoRes = await vfs.readCID(approxShape.geometry, context);
        const approxGeoText = await consumeText(approxGeoRes.stream);

        const originalStats = parseGeo(geometryText);
        const approxStats = parseGeo(approxGeoText);

        console.log(`[Test] Original Triangles: ${originalStats.triangles}, Vertices: ${originalStats.vertices}`);
        console.log(`[Test] Approximated Triangles: ${approxStats.triangles}, Vertices: ${approxStats.vertices}`);

        // Topological Assertions
        assert.ok(approxStats.triangles < originalStats.triangles, "Triangle count must be reduced");
        assert.ok(approxStats.vertices < originalStats.vertices, "Vertex count must be reduced");
        assert.strictEqual(approxStats.nonManifoldEdges, 0, "Approximated shape must have 0 non-manifold edges");

        // Verify PNG rendering is functional
        console.log("[Test] Verifying PNG render on approximated shape...");
        const approxPngSel = new Selector('jot/png', {
            $in: shapeCID,
            width: 256,
            height: 256,
            ax: 45,
            ay: 45
        }).withOutput('$out');
        const approxPngRes = await vfs.readSelector(approxPngSel, context);
        assert.ok(approxPngRes, "PNG render must return a valid VFS entry");
        const pngBytes = await consumeBinary(approxPngRes.stream);
        assert.ok(pngBytes.length > 0, "Rendered PNG must not be empty");

    } finally {
        console.log("[Test] Cleaning up...");
        mesh.stop();
        await vfs.close();
        await sys.stop();
    }
});
