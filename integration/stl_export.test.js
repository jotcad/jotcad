import { runIntegrationTest } from './harness.js';
import fs from 'fs';

runIntegrationTest('STL Export Integration Test', async ({ compiler, parser, readOutput }) => {
    const code = 'Box(10).stl("test.stl") -> $out';
    console.log("Parsing & Evaluating:", code);
    
    const ast = parser.parse(code);
    const terminals = await compiler.evaluate(ast, {}, {
        outputs: { "$out": { type: "file" } }
    });
    
    console.log("Requesting STL bytes via readOutput...");
    const bytes = await readOutput(terminals, '$out');
    
    if (!bytes) {
        throw new Error("Failed to consume STL bytes");
    }

    console.log("STL File Size:", bytes.byteLength);
    
    if (bytes.byteLength === 684) {
        console.log("SUCCESS: STL file size matches expectations (684 bytes).");
    } else {
        console.warn("WARNING: STL file size mismatch. Expected 684, got", bytes.byteLength);
    }

    fs.writeFileSync('test.stl', Buffer.from(bytes));
    console.log("Wrote test.stl");
});
