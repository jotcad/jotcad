import assert from 'node:assert';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Unfold Integration Test', async ({ readData, evaluate, readOutput, captureOutputPNG }) => {
    // Test 1: Box Unfold
    const r1 = await evaluate("Box(10, 10, 10).color('red').unfold() -> $out");
    const shape1 = await readOutput(r1);
    assert.strictEqual(shape1.components.length, 1);
    
    const pngBytes = await captureOutputPNG(r1, 'unfold_box_result.png');
    const crypto = await import('node:crypto');
    const actualHash = crypto.createHash('sha256').update(pngBytes).digest('hex');
    assert.strictEqual(actualHash, '091de468804708b16ade93cddd046d9ad7aa325a91dd04caac0ed101cb549531', 'PNG content hash mismatch for unfold_box_result.png!');

    // Test 2: Orb Unfold
    const r2 = await evaluate("Orb(1).color('blue').unfold().pack(sheet=Box(4, 4).color('grey')) -> $out");
    const shape2 = await readOutput(r2);
    assert.ok((shape2.components || []).length > 0);
    await captureOutputPNG(r2, 'unfold_orb_result.png');

    // Test 3: Hollow Box Unfold (Regression Test)
    const script3 = `
        U = Box(20, 20, 20).cut(Box(10, 10, 20)).unfold(strategy="pair")
        Cuts = U.inItem("*").has("unfold", "cut").color('red')
        Folds = U.inItem("*").has("unfold", "fold").color('green')
        Folds.and(Cuts).pack(sheet=Box(100, 100)) -> $out
    `;
    const r3 = await evaluate(script3);
    await captureOutputPNG(r3, 'unfold_hollow_box_result.png');
});
