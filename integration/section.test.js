import { VFS, MeshLink, Selector } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { captureAndVerifyPNG } from './png_helper.js';

async function consumeJSON(stream) {
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
    return JSON.parse(new TextDecoder().decode(bytes));
}

async function consumeText(stream) {
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
    return new TextDecoder().decode(bytes);
}

async function test() {
    console.log("Starting Section Integration Test...");
    
    // 1. Launch the TEST system (ops node on 9191)
    const sys = await launchSystem('test/standard');
    const opsUrl = `http://localhost:${sys.ports.ops}`;

    // 2. Setup VFS and MeshLink
    const vfs = new VFS({ id: 'section-test-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();

    // 3. Setup Jot Compiler
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();
    
    // 4. Fetch catalog via Zenoh subscription
    console.log("Waiting for geo-ops-node to connect...");
    const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
    await waitForMeshNodes(vfs, ['geo-ops-node']);

    console.log("Subscribing to sys/schema for catalog discovery...");
    let catalogReceived = null;
    vfs.events.on('notify', (selector, payload) => {
        if (selector.path === 'sys/schema') {
            catalogReceived = payload;
        }
    });

    await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);

    let attempts = 0;
    while (!catalogReceived && attempts < 50) {
        await new Promise(r => setTimeout(r, 100));
        attempts++;
    }

    if (!catalogReceived) {
        throw new Error('Should have received schema catalog');
    }
    const catalog = catalogReceived.catalog;
    for (const [name, schema] of Object.entries(catalog)) {
        compiler.registerOperator(name, { path: name, schema: schema });
    }

    try {
        // --- Test: Box(10, 20, 30).section() ---
        const code = "Box(10, 20, 30).section() -> $out";
        console.log("Evaluating Section Test:", code);
        const ast = parser.parse(code);
        
        const terminals = await compiler.evaluate(ast, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const sectionResult = terminals.find(t => t.port === '$out');
        const shape = await consumeJSON((await vfs.read(sectionResult.selector)).stream);
        
        console.log("Section Result tags:", shape.tags);
        console.log("Section Result geometry CID:", shape.geometry);

        // Save PNG capture of the section
        console.log("Capturing PNG snapshot of the section...");
        await captureAndVerifyPNG(vfs, sectionResult.selector, 'section_box_result.png');

        // Validation: Verify that the shape has type: "surface" tag
        if (!shape.tags || shape.tags.type !== 'surface') {
            throw new Error(`Expected tag type 'surface', got ${shape.tags ? shape.tags.type : 'none'}`);
        }

        // Fetch and parse the raw geometry data
        if (!shape.geometry) {
            throw new Error(`Expected section shape to have a geometry CID`);
        }
        const { stream: geoStream } = await vfs.read(shape.geometry);
        const geoText = await consumeText(geoStream);

        console.log("Section Geometry Data:\n", geoText);

        // The section of Box(10, 20, 30) at Z=0 should be a rectangle with size 10x20 in XxY.
        // It has 4 vertices: (-5, -10, 0), (5, -10, 0), (5, 10, 0), (-5, 10, 0)
        const lines = geoText.trim().split('\n');
        let vCount = 0;
        const vMatch = lines[0].match(/^V (\d+)/);
        if (vMatch) {
            vCount = parseInt(vMatch[1], 10);
        }

        console.log("Number of section vertices:", vCount);
        if (vCount < 4) {
            throw new Error(`Expected at least 4 vertices for the rectangular section, got ${vCount}`);
        }

        // Verify the coordinate values of the vertices in the geometry text
        // E.g. x should be +/-5, y +/-10, z 0
        const vertices = [];
        for (let i = 1; i <= vCount; i++) {
            const parts = lines[i].trim().split(/\s+/);
            if (parts.length >= 3) {
                vertices.push({
                    x: parseFloat(parts[0]),
                    y: parseFloat(parts[1]),
                    z: parseFloat(parts[2])
                });
            }
        }

        console.log("Parsed vertices:", vertices);
        
        let hasTopLeft = false;
        let hasTopRight = false;
        let hasBottomLeft = false;
        let hasBottomRight = false;

        for (const v of vertices) {
            // Coordinate containment check
            if (Math.abs(v.x) > 5.0 + 1e-5 || Math.abs(v.y) > 10.0 + 1e-5 || Math.abs(v.z) > 1e-5) {
                throw new Error(`Vertex coordinate bounds mismatch: ${JSON.stringify(v)}`);
            }
            
            // Corner checks
            if (Math.abs(v.x - (-5.0)) < 1e-5 && Math.abs(v.y - 10.0) < 1e-5) hasTopLeft = true;
            if (Math.abs(v.x - 5.0) < 1e-5 && Math.abs(v.y - 10.0) < 1e-5) hasTopRight = true;
            if (Math.abs(v.x - (-5.0)) < 1e-5 && Math.abs(v.y - (-10.0)) < 1e-5) hasBottomLeft = true;
            if (Math.abs(v.x - 5.0) < 1e-5 && Math.abs(v.y - (-10.0)) < 1e-5) hasBottomRight = true;
        }

        if (!hasTopLeft || !hasTopRight || !hasBottomLeft || !hasBottomRight) {
            throw new Error(`Rectangular corners missing! TL:${hasTopLeft}, TR:${hasTopRight}, BL:${hasBottomLeft}, BR:${hasBottomRight}`);
        }

        console.log("SUCCESS: Section integration test passed.");

        // --- Test 2: Hollow Orb Packed Sections ---
        const code2 = `
            Hollow = Orb(20).cut(Orb(15));
            S1 = Hollow.section(planes=Box().mz(-5));
            S2 = Hollow.section(planes=Box().mz(0));
            S3 = Hollow.section(planes=Box().mz(5));
            Group([S1, S2, S3]).pack(sheet=Box(100, 100)) -> $out
        `;
        console.log("\nEvaluating Hollow Orb Packed Sections Test:", code2);
        const ast2 = parser.parse(code2);
        
        const terminals2 = await compiler.evaluate(ast2, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const packedResult = terminals2.find(t => t.port === '$out');
        const shape2 = await consumeJSON((await vfs.read(packedResult.selector)).stream);

        console.log("Packed Slices - Geometry:", shape2.geometry);
        console.log("Packed Slices - Number of components:", (shape2.components || []).length);
        console.log("Shape2 JSON:", JSON.stringify(shape2, null, 2));
        if (!shape2.geometry && (!shape2.components || shape2.components.length === 0)) {
            throw new Error("Expected packed orb sections to yield geometry or components");
        }
        
        // Save PNG capture of the packed hollow orb slices
        console.log("Capturing PNG snapshot of the packed hollow orb slices...");
        await captureAndVerifyPNG(vfs, packedResult.selector, 'section_hollow_orb_packed.png');

        console.log("SUCCESS: Hollow Orb packed sections test passed.");
    } finally {
        console.log("Cleaning up...");
        await mesh.stop();
        await vfs.close();
        await sys.stop();
    }
}

test().catch(e => {
    console.error("Test Failed:", e.message);
    process.exit(1);
});
