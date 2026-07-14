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
    
    let minX = Infinity, maxX = -Infinity;
    let minY = Infinity, maxY = -Infinity;
    let minZ = Infinity, maxZ = -Infinity;
    
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        if (line.startsWith('V ')) {
            numVertices = parseInt(line.slice(2).trim(), 10);
            for (let j = 1; j <= numVertices; j++) {
                if (i + j >= lines.length) break;
                const parts = lines[i + j].trim().split(/\s+/).map(Number);
                if (parts.length >= 3) {
                    const [x, y, z] = parts;
                    minX = Math.min(minX, x); maxX = Math.max(maxX, x);
                    minY = Math.min(minY, y); maxY = Math.max(maxY, y);
                    minZ = Math.min(minZ, z); maxZ = Math.max(maxZ, z);
                }
            }
            i += numVertices;
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
        nonManifoldEdges,
        bbox: { minX, maxX, minY, maxY, minZ, maxZ }
    };
}

async function run() {
    // Parse alpha, offset, proxies, and tolerance from command line arguments
    const args = process.argv.slice(2);
    let alpha = 0.4;
    let offset = 0.05;
    let proxies = 120;
    let tolerance = -1.0;
    for (let i = 0; i < args.length; i++) {
        if (args[i] === '--alpha') alpha = parseFloat(args[++i]);
        else if (args[i] === '--offset') offset = parseFloat(args[++i]);
        else if (args[i] === '--proxies') proxies = parseInt(args[++i], 10);
        else if (args[i] === '--tolerance') tolerance = parseFloat(args[++i]);
    }

    console.log(`[Approximate Test] Using parameters: alpha = ${alpha}, offset = ${offset}, proxies = ${proxies}, tolerance = ${tolerance}`);
    console.log("[Approximate Test] Launching local cluster...");
    const sys = await launchSystem('test/standard');

    // Setup VFS and MeshLink
    const vfs = new VFS({ id: 'approximate-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    try {
        console.log("[Approximate Test] Reading 28byj-48_5vdc files...");
        const geoPath = path.resolve(libDir, '28byj-48_5vdc.geo');
        const shapePath = path.resolve(libDir, '28byj-48_5vdc.json');

        if (!fs.existsSync(geoPath) || !fs.existsSync(shapePath)) {
            throw new Error("Target STEP conversion files not found.");
        }

        const geometryText = fs.readFileSync(geoPath, 'utf8');
        const shapeJSONText = fs.readFileSync(shapePath, 'utf8');

        const originalStats = parseGeo(geometryText);
        const { minX, maxX, minY, maxY, minZ, maxZ } = originalStats.bbox;
        const dx = maxX - minX;
        const dy = maxY - minY;
        const dz = maxZ - minZ;
        const diagonal = Math.sqrt(dx*dx + dy*dy + dz*dz);
        console.log(`[Approximate Test] Original Bounding Box:`);
        console.log(`  - X: [${minX.toFixed(3)}, ${maxX.toFixed(3)}] (size: ${dx.toFixed(3)})`);
        console.log(`  - Y: [${minY.toFixed(3)}, ${maxY.toFixed(3)}] (size: ${dy.toFixed(3)})`);
        console.log(`  - Z: [${minZ.toFixed(3)}, ${maxZ.toFixed(3)}] (size: ${dz.toFixed(3)})`);
        console.log(`  - Diagonal: ${diagonal.toFixed(3)}`);

        const geoCID = await getCID(geometryText);
        const shapeCID = await getCID(shapeJSONText);

        // Write geometry and shape to client VFS
        await vfs.write(geoCID, geometryText, { encoding: 'string', filename: '28byj-48_5vdc.geo' });
        await vfs.write(shapeCID, shapeJSONText, { encoding: 'json', filename: '28byj-48_5vdc.json' });

        const context = { expiresAt: Date.now() + 600000 };

        // ==========================================
        // STAGE 1: jot/wrap (Watertight Manifold)
        // ==========================================
        console.log(`[Approximate Test] Running Stage 1: jot/wrap (alpha: ${alpha}, offset: ${offset})...`);
        const wrapSel = new Selector('jot/wrap', {
            $in: shapeCID,
            alpha: alpha,
            offset: offset
        }).withOutput('$out');

        const wrapResult = await vfs.readSelector(wrapSel, context);
        if (!wrapResult) {
            throw new Error("Wrap operator failed to return a result.");
        }

        const wrappedShape = await consumeJSON(wrapResult.stream);
        console.log(`[Approximate Test] Stage 1 Wrapped Shape parsed.`);

        // Read wrapped geometry
        const wrappedGeoRes = await vfs.readCID(wrappedShape.geometry, context);
        const wrappedGeoText = await consumeText(wrappedGeoRes.stream);

        // Save wrapped files
        const wrappedGeoPath = path.resolve(libDir, '28byj-48_5vdc_wrapped.geo');
        const wrappedShapePath = path.resolve(libDir, '28byj-48_5vdc_wrapped.json');
        fs.writeFileSync(wrappedGeoPath, wrappedGeoText);
        fs.writeFileSync(wrappedShapePath, JSON.stringify(wrappedShape, null, 2));

        // Write wrapped shape/geometry to client VFS for downstream tasks
        const wrappedShapeText = JSON.stringify(wrappedShape);
        const wrappedShapeCID = await getCID(wrappedShapeText);
        await vfs.write(wrappedShapeCID, wrappedShapeText, { encoding: 'json', filename: '28byj-48_5vdc_wrapped.json' });
        await vfs.write(wrappedShape.geometry, wrappedGeoText, { encoding: 'string', filename: '28byj-48_5vdc_wrapped.geo' });

        // ==========================================
        // STAGE 2: jot/approximate (VSA)
        // ==========================================
        const maxProxies = proxies;
        console.log(`[Approximate Test] Running Stage 2: jot/approximate (max_proxies: ${maxProxies}, tolerance: ${tolerance})...`);
        const approxSel = new Selector('jot/approximate', {
            $in: wrappedShapeCID,
            max_proxies: maxProxies,
            tolerance: tolerance
        }).withOutput('$out');

        const approxResult = await vfs.readSelector(approxSel, context);
        if (!approxResult) {
            throw new Error("Approximate operator failed to return a result.");
        }

        const approxShape = await consumeJSON(approxResult.stream);
        console.log(`[Approximate Test] Stage 2 Approximated Shape parsed.`);

        // Read approximated geometry
        const approxGeoRes = await vfs.readCID(approxShape.geometry, context);
        const approxGeoText = await consumeText(approxGeoRes.stream);

        // Save approximated files
        const approxGeoPath = path.resolve(libDir, '28byj-48_5vdc_approx.geo');
        const approxShapePath = path.resolve(libDir, '28byj-48_5vdc_approx.json');
        fs.writeFileSync(approxGeoPath, approxGeoText);
        fs.writeFileSync(approxShapePath, JSON.stringify(approxShape, null, 2));

        // Write approximated shape/geometry to client VFS for rendering
        const approxShapeText = JSON.stringify(approxShape);
        const approxShapeCID = await getCID(approxShapeText);
        await vfs.write(approxShapeCID, approxShapeText, { encoding: 'json', filename: '28byj-48_5vdc_approx.json' });
        await vfs.write(approxShape.geometry, approxGeoText, { encoding: 'string', filename: '28byj-48_5vdc_approx.geo' });

        // ==========================================
        // Topo Stats & Comparison
        // ==========================================
        const wrappedStats = parseGeo(wrappedGeoText);
        const approxStats = parseGeo(approxGeoText);

        console.log(`========================================================================`);
        console.log(`[Approximate Test] PIPELINE SUCCESS!`);
        console.log(`Metric                  | Original Shape | Wrapped Shape  | Approximated`);
        console.log(`------------------------+----------------+----------------+-------------`);
        console.log(`Vertices                | ${originalStats.vertices.toString().padEnd(14)} | ${wrappedStats.vertices.toString().padEnd(14)} | ${approxStats.vertices}`);
        console.log(`Triangles               | ${originalStats.triangles.toString().padEnd(14)} | ${wrappedStats.triangles.toString().padEnd(14)} | ${approxStats.triangles}`);
        console.log(`Boundary Edges          | ${originalStats.boundaryEdges.toString().padEnd(14)} | ${wrappedStats.boundaryEdges.toString().padEnd(14)} | ${approxStats.boundaryEdges}`);
        console.log(`Non-Manifold Edges      | ${originalStats.nonManifoldEdges.toString().padEnd(14)} | ${wrappedStats.nonManifoldEdges.toString().padEnd(14)} | ${approxStats.nonManifoldEdges}`);
        console.log(`========================================================================`);

        // ==========================================
        // Render All Shapes to PNG
        // ==========================================
        const renderPng = async (inCID, destFilename) => {
            const pngSel = new Selector('jot/png', {
                $in: inCID,
                width: 512,
                height: 512,
                ax: 45,
                ay: 45
            }).withOutput('$out');
            const pngRes = await vfs.readSelector(pngSel, context);
            if (pngRes) {
                const pngBytes = await consumeBinary(pngRes.stream);
                const destPath = path.resolve(libDir, destFilename);
                fs.writeFileSync(destPath, pngBytes);
                console.log(`[Approximate Test] Rendered and saved: ${destFilename}`);
            } else {
                console.warn(`[Approximate Test] Failed to render: ${destFilename}`);
            }
        };

        console.log(`[Approximate Test] Rendering images...`);
        await renderPng(shapeCID, '28byj-48_5vdc.png');
        await renderPng(wrappedShapeCID, '28byj-48_5vdc_wrapped.png');
        await renderPng(approxShapeCID, '28byj-48_5vdc_approx.png');

    } catch (err) {
        console.error(`❌ Approximate Test FAILED:`, err);
    } finally {
        console.log("[Approximate Test] Shutting down VFS and cluster...");
        await mesh.stop();
        await vfs.close();
        await new Promise(resolve => setTimeout(resolve, 1000));
        process.exit(0);
    }
}

run();
