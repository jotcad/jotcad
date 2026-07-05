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

function generateCylinderGeoText(radius, height, slices, segments) {
    const vertices = [];
    const triangles = [];
    const dz = height / slices;
    const dtheta = (2.0 * Math.PI) / segments;

    // 1. Vertices
    for (let s = 0; s <= slices; s++) {
        const z = s * dz;
        for (let i = 0; i < segments; i++) {
            const theta = i * dtheta;
            const x = radius * Math.cos(theta);
            const y = radius * Math.sin(theta);
            vertices.push(`${x} ${y} ${z}`);
        }
    }

    // 2. Side triangles
    for (let s = 0; s < slices; s++) {
        for (let i = 0; i < segments; i++) {
            const inext = (i + 1) % segments;
            const v00 = s * segments + i;
            const v01 = s * segments + inext;
            const v10 = (s + 1) * segments + i;
            const v11 = (s + 1) * segments + inext;

            triangles.push(`${v00} ${v01} ${v11}`);
            triangles.push(`${v00} ${v11} ${v10}`);
        }
    }

    // 3. Bottom cap
    const centerBottomIdx = vertices.length;
    vertices.push(`0 0 0`);
    for (let i = 0; i < segments; i++) {
        const inext = (i + 1) % segments;
        triangles.push(`${inext} ${i} ${centerBottomIdx}`);
    }

    // 4. Top cap
    const centerTopIdx = vertices.length;
    vertices.push(`0 0 ${height}`);
    const topStart = slices * segments;
    for (let i = 0; i < segments; i++) {
        const inext = (i + 1) % segments;
        triangles.push(`${topStart + i} ${topStart + inext} ${centerTopIdx}`);
    }

    let out = `V ${vertices.length}\n`;
    out += vertices.join('\n') + '\n';
    out += `T ${triangles.length}\n`;
    out += triangles.join('\n') + '\n';
    return out;
}

test('jot/rig - C++ VFS Operator & PBD Skinning Integration', { timeout: 120000 }, async (t) => {
    let sys;
    let vfs;
    let mesh;

    try {
        console.log("[Test] Launching cluster for rigging integration test...");
        sys = await launchSystem('test/standard');

        vfs = new VFS({ id: 'test-rigging-client' });
        mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
        
        await vfs.init();
        await mesh.start();

        // Wait for the C++ ops node to join the Zenoh mesh
        console.log("[Test] Waiting for mesh nodes...");
        await waitForMeshNodes(vfs, ['geo-mapping-node']);

        // 1. Generate cylinder geometry
        console.log("[Test] Generating Cylinder geometry...");
        const cylinderGeoText = generateCylinderGeoText(1.0, 10.0, 20, 16);
        const geoCID = await getCID(cylinderGeoText);
        await vfs.write(geoCID, cylinderGeoText, { encoding: 'string', filename: 'cylinder.geo' });

        // 2. Assemble the Rig Shape JSON
        const rigShape = {
            tags: { type: "rig" },
            components: [
                {
                    geometry: geoCID,
                    tags: { type: "solid" }
                },
                {
                    tf: "1 0 0 0 0 1 0 0 0 0 1 0",
                    tags: {
                        type: "joint",
                        role: "ghost",
                        joint_id: 0,
                        name: "Base",
                        bind_tf: "1 0 0 0 0 1 0 0 0 0 1 0"
                    },
                    components: [
                        {
                            // Translate(0, 0, 5) * RotateX(90 degrees)
                            tf: "1 0 0 0 0 0 -1 0 0 1 0 5",
                            tags: {
                                type: "joint",
                                role: "ghost",
                                joint_id: 1,
                                name: "Elbow",
                                bind_tf: "1 0 0 0 0 1 0 0 0 0 1 5"
                            },
                            components: []
                        }
                    ]
                }
            ]
        };

        const rigShapeText = JSON.stringify(rigShape);
        const rigShapeCID = await getCID(rigShapeText);
        await vfs.write(rigShapeCID, rigShapeText, { encoding: 'json', filename: 'cylinder_rig.json' });

        // 3. Query the jot/rig VFS operator
        console.log("[Test] Querying jot/rig VFS operator with PBD relaxation...");
        const rigSel = new Selector('jot/rig', {
            $in: rigShapeCID,
            pbd_iterations: 40
        }).withOutput('$out');

        const response = await vfs.read(rigSel);
        assert.ok(response, 'Should return a valid response');

        const result = JSON.parse(new TextDecoder().decode(await consumeBytes(response.stream)));
        console.log("TEST RESULT SHAPE:", JSON.stringify(result, null, 2));
        assert.strictEqual(result.tags.type, 'rig', 'Output container should be typed as rig');

        let deformedSolid = null;
        for (const comp of result.components) {
            if (comp.tags && comp.tags.type === 'solid') {
                deformedSolid = comp;
                break;
            }
        }
        assert.ok(deformedSolid, 'Should find the deformed solid component in rig components');
        assert.ok(deformedSolid.geometry, 'The output solid mesh should have a valid geometry CID');

        // 4. Capture a PNG image of the deformed cylinder
        console.log("[Test] Rendering deformed rig snapshot via jot/png...");
        const pngBytes = await captureAndVerifyPNG(vfs, rigSel, 'rig_deformed.png', null, {
            width: 512,
            height: 512,
            ax: 45,
            ay: 45
        });

        assert.ok(pngBytes.length > 0, "Rendered PNG should not be empty");
        console.log("[Test] Rigging Integration Test Passed Successfully!");
    } catch (err) {
        console.error("FATAL TEST EXCEPTION:");
        console.error(err.stack || err);
        throw err;
    } finally {
        console.log("[Test] Running cleanup...");
        if (mesh) await mesh.stop();
        if (vfs) await vfs.close();
        if (sys) await sys.stop();
    }
});
