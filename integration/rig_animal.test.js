import assert from 'node:assert';
import fs from 'node:fs';
import path from 'node:path';
import { getCID, Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';
import { waitForMeshNodes } from './vfs_test_helpers.js';

// Converts standard Wavefront OBJ format to JotCAD .geo text format
function convertObjToGeo(objText) {
    const lines = objText.split('\n');
    const vertices = [];
    const triangles = [];

    for (let line of lines) {
        line = line.trim();
        if (line.startsWith('v ')) {
            const parts = line.split(/\s+/).slice(1);
            vertices.push(`${parts[0]} ${parts[1]} ${parts[2]}`);
        } else if (line.startsWith('f ')) {
            const parts = line.split(/\s+/).slice(1);
            // Extract the first index from each slash-separated component
            const indices = parts.map(p => {
                const idxStr = p.split('/')[0];
                return parseInt(idxStr, 10) - 1; // 1-based to 0-based
            });
            if (indices.length === 3) {
                triangles.push(`${indices[0]} ${indices[1]} ${indices[2]}`);
            } else if (indices.length === 4) {
                // Triangulate quad
                triangles.push(`${indices[0]} ${indices[1]} ${indices[2]}`);
                triangles.push(`${indices[0]} ${indices[2]} ${indices[3]}`);
            }
        }
    }

    let out = `V ${vertices.length}\n`;
    out += vertices.join('\n') + '\n';
    out += `T ${triangles.length}\n`;
    out += triangles.join('\n') + '\n';
    return out;
}

// 3x4 Matrix helpers for absolute transforms
function multiplyAffine(A, B) {
    const a = [
        [A[0], A[1], A[2], A[3]],
        [A[4], A[5], A[6], A[7]],
        [A[8], A[9], A[10], A[11]],
        [0, 0, 0, 1]
    ];
    const b = [
        [B[0], B[1], B[2], B[3]],
        [B[4], B[5], B[6], B[7]],
        [B[8], B[9], B[10], B[11]],
        [0, 0, 0, 1]
    ];
    const c = Array(4).fill(0).map(() => Array(4).fill(0));
    for (let r = 0; r < 4; r++) {
        for (let col = 0; col < 4; col++) {
            c[r][col] = a[r][0]*b[0][col] + a[r][1]*b[1][col] + a[r][2]*b[2][col] + a[r][3]*b[3][col];
        }
    }
    return [
        c[0][0], c[0][1], c[0][2], c[0][3],
        c[1][0], c[1][1], c[1][2], c[1][3],
        c[2][0], c[2][1], c[2][2], c[2][3]
    ];
}

function matrixToString(M) {
    return M.map(x => x.toFixed(6)).join(' ');
}

runIntegrationTest('jot/rig - spot.obj (Keenan Crane) Cow Skeleton Rigging & PBD Skinning Integration', async ({ vfs, readData, capturePNG }) => {
    console.log("[Test] Waiting for mesh nodes...");
    await waitForMeshNodes(vfs, ['geo-mapping-node']);

    // 1. Load and convert spot.obj
    console.log("[Test] Loading and converting spot.obj...");
    const objPath = path.join(import.meta.dirname, 'spot.obj');
    const objText = fs.readFileSync(objPath, 'utf8');
    const spotGeoText = convertObjToGeo(objText);
    
    const geoCID = await getCID(spotGeoText);
    await vfs.write(geoCID, spotGeoText, { encoding: 'string', filename: 'spot.geo' });

    // 2. Set up Skeleton (Spine & Neck chain along X axis)
    const identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0];
    const translateMatrix = (dx, dy, dz) => [1, 0, 0, dx, 0, 1, 0, dy, 0, 0, 1, dz];
    const rotateZMatrix = (deg) => {
        const rad = deg * Math.PI / 180.0;
        const c = Math.cos(rad);
        const s = Math.sin(rad);
        return [c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0];
    };

    const worldTfs = [];
    const bindTfs = [];

    // Hips (Root)
    bindTfs.push(translateMatrix(-0.5, 0.1, 0.0));
    worldTfs.push(multiplyAffine(translateMatrix(-0.5, 0.1, 0.0), rotateZMatrix(10)));

    // Mid-Back
    bindTfs.push(multiplyAffine(bindTfs[0], translateMatrix(0.3, 0.1, 0.0)));
    worldTfs.push(multiplyAffine(worldTfs[0], multiplyAffine(translateMatrix(0.3, 0.1, 0.0), rotateZMatrix(10))));

    // Chest
    bindTfs.push(multiplyAffine(bindTfs[1], translateMatrix(0.3, 0.0, 0.0)));
    worldTfs.push(multiplyAffine(worldTfs[1], multiplyAffine(translateMatrix(0.3, 0.0, 0.0), rotateZMatrix(10))));

    // Neck
    bindTfs.push(multiplyAffine(bindTfs[2], translateMatrix(0.2, 0.1, 0.0)));
    worldTfs.push(multiplyAffine(worldTfs[2], multiplyAffine(translateMatrix(0.2, 0.1, 0.0), rotateZMatrix(20))));

    // Head
    bindTfs.push(multiplyAffine(bindTfs[3], translateMatrix(0.2, 0.2, 0.0)));
    worldTfs.push(multiplyAffine(worldTfs[3], multiplyAffine(translateMatrix(0.2, 0.2, 0.0), rotateZMatrix(20))));

    // Assemble Joint components hierarchy
    const joint4 = {
        tf: matrixToString(worldTfs[4]),
        tags: {
            type: "joint",
            role: "ghost",
            joint_id: 4,
            name: "Head",
            bind_tf: matrixToString(bindTfs[4])
        },
        components: []
    };

    const joint3 = {
        tf: matrixToString(worldTfs[3]),
        tags: {
            type: "joint",
            role: "ghost",
            joint_id: 3,
            name: "Neck",
            bind_tf: matrixToString(bindTfs[3])
        },
        components: [joint4]
    };

    const joint2 = {
        tf: matrixToString(worldTfs[2]),
        tags: {
            type: "joint",
            role: "ghost",
            joint_id: 2,
            name: "Chest",
            bind_tf: matrixToString(bindTfs[2])
        },
        components: [joint3]
    };

    const joint1 = {
        tf: matrixToString(worldTfs[1]),
        tags: {
            type: "joint",
            role: "ghost",
            joint_id: 1,
            name: "MidBack",
            bind_tf: matrixToString(bindTfs[1])
        },
        components: [joint2]
    };

    const joint0 = {
        tf: matrixToString(worldTfs[0]),
        tags: {
            type: "joint",
            role: "ghost",
            joint_id: 0,
            name: "Hips",
            bind_tf: matrixToString(bindTfs[0])
        },
        components: [joint1]
    };

    const rigShape = {
        tags: { type: "rig" },
        components: [
            {
                geometry: geoCID,
                tags: { type: "solid" }
            },
            joint0
        ]
    };

    const rigShapeText = JSON.stringify(rigShape);
    const rigShapeCID = await getCID(rigShapeText);
    await vfs.write(rigShapeCID, rigShapeText, { encoding: 'json', filename: 'spot_rig.json' });

    // 3. Query the VFS operator
    console.log("[Test] Querying jot/rig VFS operator with PBD skinning on spot...");
    const rigSel = new Selector('jot/rig', {
        $in: rigShapeCID,
        pbd_iterations: 40
    }).withOutput('$out');

    const result = await readData(rigSel);
    console.log("SPOT COW TEST RESULT SHAPE:", JSON.stringify(result, null, 2));
    assert.strictEqual(result.tags.type, 'rig', 'Output container should be rig');

    let deformedSolid = null;
    for (const comp of result.components) {
        if (comp.tags && comp.tags.type === 'solid') {
            deformedSolid = comp;
            break;
        }
    }
    assert.ok(deformedSolid, 'Should find deformed solid');
    assert.ok(deformedSolid.geometry, 'Should find geometry CID');

    // 4. Capture render snapshot
    console.log("[Test] Rendering spot snapshot via jot/png...");
    const pngBytes = await capturePNG(rigSel, 'rig_spot.png', {
        width: 512,
        height: 512,
        ax: 30,
        ay: 135 // Side-back angle to see the arching cow clearly
    });

    assert.ok(pngBytes.length > 0, "Rendered PNG should not be empty");
    console.log("[Test] Spot Rigging Integration Test Passed successfully!");
});
