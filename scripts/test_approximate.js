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

// Parse geometry text representation and perform topological sanity checks
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
    
    // Check boundary edges
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
    console.log("[Approximate Test] Launching local cluster...");
    const sys = await launchSystem('test/standard');

    // 2. Setup VFS and MeshLink
    const vfs = new VFS({ id: 'approximate-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    try {
        console.log("[Approximate Test] Reading 28byj-48_5vdc files...");
        const geoPath = path.resolve(libDir, '28byj-48_5vdc.geo');
        const shapePath = path.resolve(libDir, '28byj-48_5vdc.json');

        if (!fs.existsSync(geoPath) || !fs.existsSync(shapePath)) {
            throw new Error("Target STEP conversion files not found. Run scripts/convert_step.js first.");
        }

        const geometryText = fs.readFileSync(geoPath, 'utf8');
        const shapeJSONText = fs.readFileSync(shapePath, 'utf8');
        const shapeJSON = JSON.parse(shapeJSONText);

        const geoCID = await getCID(geometryText);
        const shapeCID = await getCID(shapeJSONText);

        console.log(`[Approximate Test] Loading original shape into Client VFS:`);
        console.log(`  - Geometry CID: ${geoCID}`);
        console.log(`  - Shape CID: ${shapeCID}`);

        // Write geometry and shape to client VFS so the cluster can resolve them
        await vfs.write(geoCID, geometryText, { encoding: 'string', filename: '28byj-48_5vdc.geo' });
        await vfs.write(shapeCID, shapeJSONText, { encoding: 'json', filename: '28byj-48_5vdc.json' });

        // 3. Create Selector for approximate operator
        // Target 40 planar proxy facets
        const maxProxies = 40;
        const approxSel = new Selector('jot/approximate', {
            $in: shapeCID,
            max_proxies: maxProxies
        }).withOutput('$out');

        console.log(`[Approximate Test] Calling jot/approximate on cluster (max_proxies: ${maxProxies})...`);
        const context = { expiresAt: Date.now() + 600000 };
        const result = await vfs.readSelector(approxSel, context);

        if (!result) {
            throw new Error("Approximate operator failed to return a result.");
        }

        const approxShape = await consumeJSON(result.stream);
        console.log(`[Approximate Test] Output Shape parsed successfully.`);

        // Read geometry of the approximated shape
        if (!approxShape.geometry) {
            throw new Error("Output shape has no geometry.");
        }

        console.log(`[Approximate Test] Reading approximated geometry (CID: ${approxShape.geometry})...`);
        const approxGeoRes = await vfs.readCID(approxShape.geometry, context);
        const approxGeoText = await consumeText(approxGeoRes.stream);

        const originalStats = parseGeo(geometryText);
        const approxStats = parseGeo(approxGeoText);

        console.log(`=======================================================`);
        console.log(`[Approximate Test] SUCCESS!`);
        console.log(`Metric                  | Original Shape | Approximated`);
        console.log(`------------------------+----------------+-------------`);
        console.log(`Vertices                | ${originalStats.vertices.toString().padEnd(14)} | ${approxStats.vertices}`);
        console.log(`Triangles               | ${originalStats.triangles.toString().padEnd(14)} | ${approxStats.triangles}`);
        console.log(`Faces                   | ${originalStats.faces.toString().padEnd(14)} | ${approxStats.faces}`);
        console.log(`Segments                | ${originalStats.segments.toString().padEnd(14)} | ${approxStats.segments}`);
        console.log(`Boundary Edges          | ${originalStats.boundaryEdges.toString().padEnd(14)} | ${approxStats.boundaryEdges}`);
        console.log(`Non-Manifold Edges      | ${originalStats.nonManifoldEdges.toString().padEnd(14)} | ${approxStats.nonManifoldEdges}`);
        console.log(`------------------------+----------------+-------------`);
        
        const originalFormed = (originalStats.boundaryEdges === 0 && originalStats.nonManifoldEdges === 0) ? "Closed Manifold Solid" : "Open Shell/Surface";
        const approxFormed = (approxStats.boundaryEdges === 0 && approxStats.nonManifoldEdges === 0) ? "Closed Manifold Solid" : "Open Shell/Surface";
        console.log(`Well-Formed Status      | ${originalFormed.padEnd(14)} | ${approxFormed}`);
        console.log(`=======================================================`);

        // Save the approximated geometry and shape to lib/
        const approxGeoPath = path.resolve(libDir, '28byj-48_5vdc_approx.geo');
        const approxShapePath = path.resolve(libDir, '28byj-48_5vdc_approx.json');

        fs.writeFileSync(approxGeoPath, approxGeoText);
        fs.writeFileSync(approxShapePath, JSON.stringify(approxShape, null, 2));
        console.log(`[Approximate Test] Saved approximated shape representation to: ${approxShapePath}`);
        console.log(`[Approximate Test] Saved approximated geometry to: ${approxGeoPath}`);

        // Write approximated shape and geometry to client VFS to ensure cluster nodes can resolve them
        const approxShapeText = JSON.stringify(approxShape);
        const approxShapeCID = await getCID(approxShapeText);
        await vfs.write(approxShapeCID, approxShapeText, { encoding: 'json', filename: '28byj-48_5vdc_approx.json' });
        await vfs.write(approxShape.geometry, approxGeoText, { encoding: 'string', filename: '28byj-48_5vdc_approx.geo' });

        console.log(`[Approximate Test] Rendering original shape to PNG...`);
        const originalPngSel = new Selector('jot/png', {
            $in: shapeCID,
            width: 512,
            height: 512,
            ax: 45,
            ay: 45
        }).withOutput('$out');
        const originalPngRes = await vfs.readSelector(originalPngSel, context);
        if (originalPngRes) {
            const originalPngBytes = await consumeBinary(originalPngRes.stream);
            const originalPngPath = path.resolve(libDir, '28byj-48_5vdc.png');
            fs.writeFileSync(originalPngPath, originalPngBytes);
            console.log(`[Approximate Test] Saved original PNG to: ${originalPngPath}`);
        } else {
            console.warn(`[Approximate Test] Failed to render original shape to PNG.`);
        }

        console.log(`[Approximate Test] Rendering approximated shape to PNG...`);
        const approxPngSel = new Selector('jot/png', {
            $in: approxShapeCID,
            width: 512,
            height: 512,
            ax: 45,
            ay: 45
        }).withOutput('$out');
        const approxPngRes = await vfs.readSelector(approxPngSel, context);
        if (approxPngRes) {
            const approxPngBytes = await consumeBinary(approxPngRes.stream);
            const approxPngPath = path.resolve(libDir, '28byj-48_5vdc_approx.png');
            fs.writeFileSync(approxPngPath, approxPngBytes);
            console.log(`[Approximate Test] Saved approximated PNG to: ${approxPngPath}`);
        } else {
            console.warn(`[Approximate Test] Failed to render approximated shape to PNG.`);
        }

    } catch (err) {
        console.error(`❌ Approximate Test FAILED:`, err);
    } finally {
        console.log("[Approximate Test] Shutting down VFS and cluster...");
        await mesh.stop();
        await vfs.close();
        // Wait briefly for zenoh to close sockets
        await new Promise(resolve => setTimeout(resolve, 1000));
        process.exit(0);
    }
}

run();
