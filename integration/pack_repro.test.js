import { runIntegrationTest } from './harness.js';

runIntegrationTest('Pack Repro: Triangle(20).dup(1).pack(sheet=Box(30,30))', async ({ evaluate, captureOutputPNG }) => {
    const code = "Triangle(20).color('blue').dup(20).pack(sheet=Disk(100).color('red'), rotations=2) -> $out";
    const res = await evaluate(code);
    await captureOutputPNG(res, 'pack_repro_result.png');
});
