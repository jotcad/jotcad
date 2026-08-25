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

async function run() {
    console.log("[Simplify Test] Launching local cluster...");
    const sys = await launchSystem('test/standard');

    const vfs = new VFS({ id: 'simplify-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    try {
        console.log("[Simplify Test] Reading original 28byj-48_5vdc files...");
        const geoPath = path.resolve(libDir, '28byj-48_5vdc.geo');
        const shapePath = path.resolve(libDir, '28byj-48_5vdc.json');

        if (!fs.existsSync(geoPath) || !fs.existsSync(shapePath)) {
            throw new Error("Target files not found.");
        }

        const geometryText = fs.readFileSync(geoPath, 'utf8');
        const shapeJSONText = fs.readFileSync(shapePath, 'utf8');

        const geoCID = await getCID(geometryText);
        const shapeCID = await getCID(shapeJSONText);

        await vfs.write(geoCID, geometryText, { encoding: 'string', filename: '28byj-48_5vdc.geo' });
        await vfs.write(shapeCID, shapeJSONText, { encoding: 'json', filename: '28byj-48_5vdc.json' });

        // Simplify to 10% of triangles, protecting sharp features (> 60 deg)
        const ratio = 0.1;
        const simplifySel = new Selector('jot/simplify', {
            $in: shapeCID,
            ratio: ratio,
            threshold: 60.0 / 360.0
        }).withOutput('$out');

        console.log(`[Simplify Test] Calling jot/simplify on cluster (ratio: ${ratio})...`);
        const context = { expiresAt: Date.now() + 600000 };
        const result = await vfs.readSelector(simplifySel, context);

        if (!result) {
            throw new Error("Simplify operator failed.");
        }

        const simplifiedShape = await consumeJSON(result.stream);
        const simplifiedGeoRes = await vfs.readCID(simplifiedShape.geometry, context);
        const simplifiedGeoText = await consumeText(simplifiedGeoRes.stream);

        const originalStats = parseGeo(geometryText);
        const simplifiedStats = parseGeo(simplifiedGeoText);

        console.log(`=======================================================`);
        console.log(`[Simplify Test] SUCCESS!`);
        console.log(`Metric                  | Original Shape | Simplified`);
        console.log(`------------------------+----------------+-------------`);
        console.log(`Vertices                | ${originalStats.vertices.toString().padEnd(14)} | ${simplifiedStats.vertices}`);
        console.log(`Triangles               | ${originalStats.triangles.toString().padEnd(14)} | ${simplifiedStats.triangles}`);
        console.log(`Non-Manifold Edges      | ${originalStats.nonManifoldEdges.toString().padEnd(14)} | ${simplifiedStats.nonManifoldEdges}`);
        console.log(`=======================================================`);

        // Save simplified files
        const simplifiedGeoPath = path.resolve(libDir, '28byj-48_5vdc_simple.geo');
        const simplifiedShapePath = path.resolve(libDir, '28byj-48_5vdc_simple.json');
        fs.writeFileSync(simplifiedGeoPath, simplifiedGeoText);
        fs.writeFileSync(simplifiedShapePath, JSON.stringify(simplifiedShape, null, 2));

        // Register simplified shape in client VFS for rendering
        const simpleShapeText = JSON.stringify(simplifiedShape);
        const simpleShapeCID = await getCID(simpleShapeText);
        await vfs.write(simpleShapeCID, simpleShapeText, { encoding: 'json', filename: '28byj-48_5vdc_simple.json' });
        await vfs.write(simplifiedShape.geometry, simplifiedGeoText, { encoding: 'string', filename: '28byj-48_5vdc_simple.geo' });

        console.log(`[Simplify Test] Rendering simplified shape to PNG...`);
        const simplePngSel = new Selector('jot/png', {
            $in: simpleShapeCID,
            width: 512,
            height: 512,
            ax: 45,
            ay: 45
        }).withOutput('$out');
        const simplePngRes = await vfs.readSelector(simplePngSel, context);
        if (simplePngRes) {
            const simplePngBytes = await consumeBinary(simplePngRes.stream);
            const simplePngPath = path.resolve(libDir, '28byj-48_5vdc_simple.png');
            fs.writeFileSync(simplePngPath, simplePngBytes);
            console.log(`[Simplify Test] Saved simplified PNG to: ${simplePngPath}`);
        }

    } catch (err) {
        console.error(`❌ Simplify Test FAILED:`, err);
    } finally {
        console.log("[Simplify Test] Shutting down VFS and cluster...");
        await mesh.stop();
        await vfs.close();
        await new Promise(resolve => setTimeout(resolve, 1000));
        process.exit(0);
    }
}

run();
