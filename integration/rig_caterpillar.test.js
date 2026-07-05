import test from 'node:test';
import assert from 'node:assert';
import { VFS, MeshLink, getCID, Selector } from '../fs/src/index.js';
import { launchSystem } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';
import { waitForMeshNodes } from './vfs_test_helpers.js';

async function consumeBytes(stream) {
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
    for (const chunk of chunks) {
        bytes.set(chunk, offset);
        offset += chunk.length;
    }
    return bytes;
}

// Procedural generator for a bulbous watertight caterpillar mesh along the Z axis
function generateCaterpillarGeoText(length, segments, slices) {
    const vertices = [];
    const triangles = [];
    const dz = length / slices;
    const dtheta = (2.0 * Math.PI) / segments;

    // 1. Bottom Cap Center
    vertices.push("0 0 0");

    // 2. Ring Vertices
    for (let s = 1; s < slices; s++) {
        const z = s * dz;
        // 5 bulbous segments over length 10 -> period is 2.0 units
        const r = 1.0 + 0.3 * Math.sin((Math.PI * z) / 1.0);
        for (let i = 0; i < segments; i++) {
            const theta = i * dtheta;
            const x = r * Math.cos(theta);
            const y = r * Math.sin(theta);
            vertices.push(`${x.toFixed(6)} ${y.toFixed(6)} ${z.toFixed(6)}`);
        }
    }

    // 3. Top Cap Center
    vertices.push(`0 0 ${length.toFixed(6)}`);

    // 4. Bottom Cap Triangles (connects center index 0 to ring slice s=1)
    for (let i = 0; i < segments; i++) {
        const inext = (i + 1) % segments;
        const v0 = 1 + i;
        const v1 = 1 + inext;
        triangles.push(`0 ${v1} ${v0}`);
    }

    // 5. Side Ring Triangles (connects slice s to s+1)
    for (let s = 1; s < slices - 1; s++) {
        for (let i = 0; i < segments; i++) {
            const inext = (i + 1) % segments;
            const v00 = 1 + (s - 1) * segments + i;
            const v01 = 1 + (s - 1) * segments + inext;
            const v10 = 1 + s * segments + i;
            const v11 = 1 + s * segments + inext;

            triangles.push(`${v00} ${v01} ${v11}`);
            triangles.push(`${v00} ${v11} ${v10}`);
        }
    }

    // 6. Top Cap Triangles (connects last ring slice to top center)
    const topCenterIdx = vertices.length - 1;
    const lastRingStart = 1 + (slices - 2) * segments;
    for (let i = 0; i < segments; i++) {
        const inext = (i + 1) % segments;
        const v0 = lastRingStart + i;
        const v1 = lastRingStart + inext;
        triangles.push(`${v0} ${v1} ${topCenterIdx}`);
    }

    let out = `V ${vertices.length}\n`;
    out += vertices.join('\n') + '\n';
    out += `T ${triangles.length}\n`;
    out += triangles.join('\n') + '\n';
    return out;
}

// 3x4 Matrix helpers to satisfy Independent Matrix Mandate
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

test('jot/rig - Caterpillar S-Curve Organic Rigging Integration', { timeout: 120000 }, async (t) => {
    let sys;
    let vfs;
    let mesh;

    try {
        console.log("[Test] Launching cluster for caterpillar rigging integration test...");
        sys = await launchSystem('test/standard');

        vfs = new VFS({ id: 'test-rigging-caterpillar-client' });
        mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
        
        await vfs.init();
        await mesh.start();

        console.log("[Test] Waiting for mesh nodes...");
        await waitForMeshNodes(vfs, ['geo-mapping-node']);

        // 1. Generate caterpillar geometry
        console.log("[Test] Generating Caterpillar geometry...");
        const caterpillarGeoText = generateCaterpillarGeoText(10.0, 16, 40);
        const geoCID = await getCID(caterpillarGeoText);
        await vfs.write(geoCID, caterpillarGeoText, { encoding: 'string', filename: 'caterpillar.geo' });

        // 2. Compute Skeleton Pose Matrices (S-curve arching shape)
        const identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0];
        const translateMatrix = (dz) => [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, dz];
        const rotateXMatrix = (deg) => {
            const rad = deg * Math.PI / 180.0;
            const c = Math.cos(rad);
            const s = Math.sin(rad);
            return [1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0];
        };

        const angles = [15, 15, 15, -15, -15, -15];
        const worldTfs = [];
        const bindTfs = [];

        // Build bind poses (straight along Z-axis)
        let currentBind = identity;
        for (let i = 0; i <= 5; i++) {
            bindTfs.push(currentBind);
            currentBind = multiplyAffine(currentBind, translateMatrix(2.0));
        }

        // Build deformed world poses (S-curve bend)
        let currentWorld = rotateXMatrix(angles[0]);
        worldTfs.push(currentWorld);
        for (let i = 1; i <= 5; i++) {
            const localStep = multiplyAffine(rotateXMatrix(angles[i]), translateMatrix(2.0));
            currentWorld = multiplyAffine(currentWorld, localStep);
            worldTfs.push(currentWorld);
        }

        // Assemble Joint components hierarchy
        const joint5 = {
            tf: matrixToString(worldTfs[5]),
            tags: {
                type: "joint",
                role: "ghost",
                joint_id: 5,
                name: "Head",
                bind_tf: matrixToString(bindTfs[5])
            },
            components: []
        };

        const joint4 = {
            tf: matrixToString(worldTfs[4]),
            tags: {
                type: "joint",
                role: "ghost",
                joint_id: 4,
                name: "Neck",
                bind_tf: matrixToString(bindTfs[4])
            },
            components: [joint5]
        };

        const joint3 = {
            tf: matrixToString(worldTfs[3]),
            tags: {
                type: "joint",
                role: "ghost",
                joint_id: 3,
                name: "Chest",
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
                name: "Mid",
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
                name: "Waist",
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
                name: "Base",
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
        await vfs.write(rigShapeCID, rigShapeText, { encoding: 'json', filename: 'caterpillar_rig.json' });

        // 3. Query the VFS operator
        console.log("[Test] Querying jot/rig VFS operator with PBD skinning...");
        const rigSel = new Selector('jot/rig', {
            $in: rigShapeCID,
            pbd_iterations: 40
        }).withOutput('$out');

        const response = await vfs.read(rigSel);
        assert.ok(response, 'Should return a valid response');

        const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
        console.log("CATERPILLAR TEST RESULT SHAPE:", JSON.stringify(result, null, 2));
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
        console.log("[Test] Rendering caterpillar snapshot via jot/png...");
        const pngBytes = await captureAndVerifyPNG(vfs, rigSel, 'rig_caterpillar.png', null, {
            width: 512,
            height: 512,
            ax: 45,
            ay: 45
        });

        assert.ok(pngBytes.length > 0, "Rendered PNG should not be empty");
        console.log("[Test] Caterpillar Rigging Test Passed successfully!");

    } catch (err) {
        console.error("FATAL CATERPILLAR TEST EXCEPTION:");
        console.error(err.stack || err);
        throw err;
    } finally {
        console.log("[Test] Running cleanup...");
        if (mesh) await mesh.stop();
        if (vfs) await vfs.close();
        if (sys) await sys.stop();
    }
});
