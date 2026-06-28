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

if (parentPort) {
    parentPort.on('message', async (message) => {
        try {
            const { fileBytes, deflection } = message;
            const result = await runTriangulation({ fileBytes, deflection });
            parentPort.postMessage({ success: true, ...result });
        } catch (err) {
            parentPort.postMessage({ success: false, error: err.stack || err.message });
        }
    });
}
