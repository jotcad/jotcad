import assert from 'node:assert';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Section Integration Test', async ({ readData, evaluate, readOutput, captureOutputPNG }) => {
    // Test 1: Box(10, 20, 30).section()
    const r1 = await evaluate("Box(10, 20, 30).section() -> $out");
    const shape = await readOutput(r1);
    
    assert.strictEqual(shape.tags?.type, 'surface');
    await captureOutputPNG(r1, 'section_box_result.png');

    // Parse the raw geometry data
    const geoText = await readData(shape.geometry);

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
        if (Math.abs(v.x) > 5.0 + 1e-5 || Math.abs(v.y) > 10.0 + 1e-5 || Math.abs(v.z) > 1e-5) {
            throw new Error(`Vertex coordinate bounds mismatch: ${JSON.stringify(v)}`);
        }
        
        if (Math.abs(v.x - (-5.0)) < 1e-5 && Math.abs(v.y - 10.0) < 1e-5) hasTopLeft = true;
        if (Math.abs(v.x - 5.0) < 1e-5 && Math.abs(v.y - 10.0) < 1e-5) hasTopRight = true;
        if (Math.abs(v.x - (-5.0)) < 1e-5 && Math.abs(v.y - (-10.0)) < 1e-5) hasBottomLeft = true;
        if (Math.abs(v.x - 5.0) < 1e-5 && Math.abs(v.y - (-10.0)) < 1e-5) hasBottomRight = true;
    }

    if (!hasTopLeft || !hasTopRight || !hasBottomLeft || !hasBottomRight) {
        throw new Error(`Rectangular corners missing! TL:${hasTopLeft}, TR:${hasTopRight}, BL:${hasBottomLeft}, BR:${hasBottomRight}`);
    }

    console.log("SUCCESS: Section integration test passed.");

    // Test 2: Hollow Orb Packed Sections
    const script2 = `
        Hollow = Orb(20).cut(Orb(15))
        S1 = Hollow.section(planes=Box().mz(-5))
        S2 = Hollow.section(planes=Box().mz(0))
        S3 = Hollow.section(planes=Box().mz(5))
        Group([S1, S2, S3]).pack(sheet=Box(100, 100)) -> $out
    `;
    const r2 = await evaluate(script2);
    const shape2 = await readOutput(r2);

    console.log("Packed Slices - Geometry:", shape2.geometry);
    if (!shape2.geometry && (!shape2.components || shape2.components.length === 0)) {
        throw new Error("Expected packed orb sections to yield geometry or components");
    }
    
    await captureOutputPNG(r2, 'section_hollow_orb_packed.png');
    console.log("SUCCESS: Hollow Orb packed sections test passed.");
});
