import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';

runIntegrationTest('Icosahedron Primitive: Diameter, Edge Length & Boolean Cut', async ({ evaluate, captureOutputPNG }) => {
    // 1. Basic icosahedron by diameter
    const res1 = await evaluate("Icosahedron(38.042).color('gold') -> $out");
    await captureOutputPNG(res1, 'icosahedron_diameter.png');

    // 2. Icosahedron by explicit edge length
    const res2 = await evaluate("Icosahedron(edge=20).color('cornflowerblue') -> $out");
    await captureOutputPNG(res2, 'icosahedron_edge.png');

    // 3. Hollow Icosahedron shell: outer cut inner
    const res3 = await evaluate("Icosahedron(edge=26).color('silver').cut(Icosahedron(edge=20.2)) -> $out");
    await captureOutputPNG(res3, 'icosahedron_hollow.png');
});
