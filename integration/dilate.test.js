import { runIntegrationTest } from './harness.js';

runIntegrationTest('Dilate Operator End-to-End', {
    script: `
        b = Box(10, 10, 10)
        d = b.dilate(4.0)
        Group(
            b.move([-8, 0, 0]).color("#2196F3"),
            d.move([8, 0, 0]).color("#FF9800")
        ) -> $out
    `,
    png: 'dilate_box_comparison.png',
    pngOptions: {
        ax: -0.61547,
        ay: 0.78539
    }
});
