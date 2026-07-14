import { parentPort, workerData } from 'node:worker_threads';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createRequire } from 'node:module';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Lazy loaded OpenCascade instance inside the worker thread
let ocPromise = null;
let ocInstance = null;

async function getOpenCascade() {
    if (ocPromise) return ocPromise;
    ocPromise = (async () => {
        const wasmPath = path.resolve(__dirname, '../node_modules/opencascade.js/dist/opencascade.wasm.wasm');
        const jsPath = path.resolve(__dirname, '../node_modules/opencascade.js/dist/opencascade.wasm.js');
        const wasmBinary = fs.readFileSync(wasmPath);

        // Polyfill Node CJS globals in ESM inside the worker
        globalThis.__dirname = path.dirname(jsPath);
        const require = createRequire(import.meta.url);
        globalThis.require = require;

        const opencascade = (await import(jsPath)).default;
        ocInstance = await opencascade({ wasmBinary });
        return ocInstance;
    })();
    return ocPromise;
}

async function runTriangulation({ fileBytes, deflection }) {
    const oc = await getOpenCascade();
    
    oc.FS.writeFile("import.stp", fileBytes);
    const reader = new oc.STEPControl_Reader_1();
    const status = reader.ReadFile("import.stp");
    if (status !== oc.IFSelect_ReturnStatus.IFSelect_RetDone) {
        oc.FS.unlink("import.stp");
        reader.delete();
        throw new Error(`Failed to read STEP file, status: ${status}`);
    }
    
    reader.TransferRoots();
    const shape = reader.OneShape();
    oc.FS.unlink("import.stp");

    // Shape topology classification
    let typeTag = "closed";
    const st = shape.ShapeType();
    if (st === oc.TopAbs_ShapeEnum.TopAbs_FACE) {
        typeTag = "surface";
    } else if (st === oc.TopAbs_ShapeEnum.TopAbs_SHELL) {
        typeTag = "open";
    } else if (st === oc.TopAbs_ShapeEnum.TopAbs_SOLID) {
        typeTag = "closed";
    }

    // Meshing
    const mesh = new oc.BRepMesh_IncrementalMesh_2(shape, deflection, false, 0.5, false);
    const explorer = new oc.TopExp_Explorer_2(shape, oc.TopAbs_ShapeEnum.TopAbs_FACE, oc.TopAbs_ShapeEnum.TopAbs_SHAPE);
    
    const vertexMap = new Map();
    const globalVertices = [];
    const globalTriangles = [];

    function getOrCreateVertex(x, y, z) {
        const key = `${x.toFixed(10)},${y.toFixed(10)},${z.toFixed(10)}`;
        if (vertexMap.has(key)) {
            return vertexMap.get(key);
        }
        const idx = globalVertices.length;
        globalVertices.push([x, y, z]);
        vertexMap.set(key, idx);
        return idx;
    }

    while (explorer.More()) {
        const face = oc.TopoDS.Face_1(explorer.Current());
        const loc = new oc.TopLoc_Location_1();
        const triangulation = oc.BRep_Tool.Triangulation(face, loc);
        
        if (!triangulation.IsNull()) {
            const triObj = triangulation.get();
            const nNodes = triObj.NbNodes();
            const nTris = triObj.NbTriangles();
            const trsf = loc.Transformation();
            
            const localToGlobal = {};
            for (let i = 1; i <= nNodes; i++) {
                const pnt = triObj.Node(i);
                const transPnt = pnt.Transformed(trsf);
                const gIdx = getOrCreateVertex(transPnt.X(), transPnt.Y(), transPnt.Z());
                localToGlobal[i] = gIdx;

                pnt.delete();
                transPnt.delete();
            }
            
            for (let j = 1; j <= nTris; j++) {
                const triangle = triObj.Triangle(j);
                const idx1 = triangle.Value(1);
                const idx2 = triangle.Value(2);
                const idx3 = triangle.Value(3);
                
                globalTriangles.push([localToGlobal[idx1], localToGlobal[idx2], localToGlobal[idx3]]);
                triangle.delete();
            }
            trsf.delete();
        }
        loc.delete();
        face.delete();
        explorer.Next();
    }

    mesh.delete();
    explorer.delete();
    reader.delete();
    shape.delete();

    // Format text Geometry representation (V F P S T)
    let geometryText = `V ${globalVertices.length}\n`;
    for (const v of globalVertices) {
        geometryText += `${v[0]} ${v[1]} ${v[2]}\n`;
    }
    geometryText += `F 0\n`;
    geometryText += `P 0\n`;
    geometryText += `S 0\n`;
    geometryText += `T ${globalTriangles.length}\n`;
    for (const t of globalTriangles) {
        geometryText += `${t[0]} ${t[1]} ${t[2]}\n`;
    }

    return { geometryText, typeTag };
}

async function runSvgTriangulation({ faces, deflection }) {
    const oc = await getOpenCascade();
    const results = [];

    for (let fIdx = 0; fIdx < faces.length; fIdx++) {
        const faceData = faces[fIdx];
        
        let outerWire;
        try {
            outerWire = makeWireFromPoints(oc, faceData.outer);
        } catch (e) {
            console.error('[OCCT Svg Worker] Failed to build outer wire:', e);
            continue;
        }

        if (outerWire.IsNull()) {
            outerWire.delete();
            continue;
        }

        const faceMaker = new oc.BRepBuilderAPI_MakeFace_15(outerWire, true);
        
        const holeWires = [];
        for (const hole of faceData.holes) {
            try {
                const hw = makeWireFromPoints(oc, hole);
                if (!hw.IsNull()) {
                    faceMaker.Add(hw);
                    holeWires.push(hw);
                }
            } catch (e) {
                console.error('[OCCT Svg Worker] Failed to build hole wire:', e);
            }
        }

        const shape = faceMaker.Face();
        if (shape.IsNull()) {
            outerWire.delete();
            for (const hw of holeWires) hw.delete();
            faceMaker.delete();
            continue;
        }

        // Triangulate this face
        const mesh = new oc.BRepMesh_IncrementalMesh_2(shape, deflection, false, 0.5, false);

        const vertexMap = new Map();
        const vertices = [];
        const triangles = [];
        const segments = [];

        // Collect triangulation
        const loc = new oc.TopLoc_Location_1();
        const triangulation = oc.BRep_Tool.Triangulation(shape, loc);
        
        if (!triangulation.IsNull()) {
            const triObj = triangulation.get();
            const nNodes = triObj.NbNodes();
            const nTris = triObj.NbTriangles();
            const trsf = loc.Transformation();
            
            const localToGlobal = {};
            for (let i = 1; i <= nNodes; i++) {
                const pnt = triObj.Node(i);
                const transPnt = pnt.Transformed(trsf);
                
                const x = transPnt.X();
                const y = transPnt.Y();
                const z = transPnt.Z();
                
                const key = `${x.toFixed(10)},${y.toFixed(10)},${z.toFixed(10)}`;
                let gIdx;
                if (vertexMap.has(key)) {
                    gIdx = vertexMap.get(key);
                } else {
                    gIdx = vertices.length;
                    vertices.push([x, y, z]);
                    vertexMap.set(key, gIdx);
                }
                localToGlobal[i] = gIdx;

                pnt.delete();
                transPnt.delete();
            }
            
            for (let j = 1; j <= nTris; j++) {
                const triangle = triObj.Triangle(j);
                const idx1 = triangle.Value(1);
                const idx2 = triangle.Value(2);
                const idx3 = triangle.Value(3);
                
                triangles.push([localToGlobal[idx1], localToGlobal[idx2], localToGlobal[idx3]]);
                triangle.delete();
            }
            trsf.delete();
        }
        loc.delete();

        // Also add the segment lines for the boundaries
        function addLoopSegments(loopPoints) {
            const loopIndices = [];
            for (const pt of loopPoints) {
                const x = pt[0];
                const y = -pt[1];
                const z = 0.0;
                const key = `${x.toFixed(10)},${y.toFixed(10)},${z.toFixed(10)}`;
                let gIdx;
                if (vertexMap.has(key)) {
                    gIdx = vertexMap.get(key);
                } else {
                    gIdx = vertices.length;
                    vertices.push([x, y, z]);
                    vertexMap.set(key, gIdx);
                }
                loopIndices.push(gIdx);
            }
            for (let i = 0; i < loopIndices.length - 1; i++) {
                segments.push([loopIndices[i], loopIndices[i+1]]);
            }
            // Close loop
            if (loopIndices.length > 1) {
                segments.push([loopIndices[loopIndices.length - 1], loopIndices[0]]);
            }
        }

        addLoopSegments(faceData.outer);
        for (const hole of faceData.holes) {
            addLoopSegments(hole);
        }

        // Format geometryText
        let geometryText = `V ${vertices.length}\n`;
        for (const v of vertices) {
            geometryText += `${v[0]} ${v[1]} ${v[2]}\n`;
        }
        geometryText += `F 0\n`;
        geometryText += `P 0\n`;
        geometryText += `S ${segments.length}\n`;
        for (const s of segments) {
            geometryText += `${s[0]} ${s[1]}\n`;
        }
        geometryText += `T ${triangles.length}\n`;
        for (const t of triangles) {
            geometryText += `${t[0]} ${t[1]} ${t[2]}\n`;
        }

        results.push({ geometryText });

        mesh.delete();
        shape.delete();
        faceMaker.delete();
        outerWire.delete();
        for (const hw of holeWires) hw.delete();
    }

    return { results };
}

function makeWireFromPoints(oc, points) {
    const wireMaker = new oc.BRepBuilderAPI_MakeWire_1();
    for (let i = 0; i < points.length - 1; i++) {
        const p1 = new oc.gp_Pnt_3(points[i][0], -points[i][1], 0);
        const p2 = new oc.gp_Pnt_3(points[i+1][0], -points[i+1][1], 0);
        const edgeMaker = new oc.BRepBuilderAPI_MakeEdge_3(p1, p2);
        if (!edgeMaker.Edge().IsNull()) {
            wireMaker.Add_1(edgeMaker.Edge());
        }
        p1.delete();
        p2.delete();
        edgeMaker.delete();
    }
    const first = points[0];
    const last = points[points.length - 1];
    if (Math.hypot(first[0] - last[0], first[1] - last[1]) > 1e-5) {
        const p1 = new oc.gp_Pnt_3(last[0], -last[1], 0);
        const p2 = new oc.gp_Pnt_3(first[0], -first[1], 0);
        const edgeMaker = new oc.BRepBuilderAPI_MakeEdge_3(p1, p2);
        if (!edgeMaker.Edge().IsNull()) {
            wireMaker.Add_1(edgeMaker.Edge());
        }
        p1.delete();
        p2.delete();
        edgeMaker.delete();
    }
    const wire = wireMaker.Wire();
    wireMaker.delete();
    return wire;
}

if (parentPort) {
    parentPort.on('message', async (message) => {
        try {
            if (message.type === 'svg') {
                const { faces, deflection } = message;
                const result = await runSvgTriangulation({ faces, deflection });
                parentPort.postMessage({ success: true, ...result });
            } else {
                const { fileBytes, deflection } = message;
                const result = await runTriangulation({ fileBytes, deflection });
                parentPort.postMessage({ success: true, ...result });
            }
        } catch (err) {
            parentPort.postMessage({ success: false, error: err.stack || err.message });
        }
    });
}
