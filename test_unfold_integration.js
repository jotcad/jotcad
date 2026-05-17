import { VFS, MeshLink } from './fs/src/index.js';
import { JotCompiler } from './jot/src/compiler.js';
import { JotParser } from './jot/src/parser.js';
import { launchSystem, PROFILES } from './orchestrator.js';

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

async function test() {
    console.log("Starting Unfold Integration Test...");
    
    // 1. Launch the TEST system (ops node on 9191)
    const sys = await launchSystem(PROFILES.TEST);
    const opsUrl = `http://localhost:${sys.ports.ops}`;

    // 2. Setup VFS and MeshLink
    const vfs = new VFS({ id: 'test-client' });
    const mesh = new MeshLink(vfs, [opsUrl]);
    
    await vfs.init();
    await mesh.start();

    // 3. Setup Jot Compiler
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();
    
    // Manual registration with FORMAL schemas
    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [
                { name: 'width', type: 'jot:interval', default: 10.0 },
                { name: 'height', type: 'jot:interval', default: 10.0 },
                { name: 'depth', type: 'jot:interval', default: 10.0 }
            ],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    compiler.registerOperator('unfold', {
        path: 'jot/unfold',
        schema: {
            arguments: [{ name: '$in', type: 'jot:shape' }],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    try {
        const code = 'Box(10).unfold()';
        console.log("Evaluating:", code);
        const ast = parser.parse(code);
        
        const terminals = await compiler.evaluate(ast, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const unfoldResult = terminals.find(t => t.port === '$out');
        if (!unfoldResult) {
            throw new Error("Failed to find unfold terminal output");
        }

        console.log("Fetching Unfold result via vfs.read()...");
        const result = await vfs.read(unfoldResult.selector);
        if (!result) throw new Error("VFS read returned null");

        const shape = await consumeJSON(result.stream);
        
        console.log("Result Tags:", JSON.stringify(shape.tags));
        console.log("Number of islands:", (shape.components || []).length);

        // Verification: A cube (Box(10)) has 6 faces.
        if (!shape.components || shape.components.length !== 6) {
            throw new Error(`Expected 6 islands for a cube, got ${shape.components ? shape.components.length : 0}`);
        }

        console.log("SUCCESS: Unfold integration test passed.");
    } finally {
        console.log("Cleaning up...");
        mesh.stop();
        await vfs.close();
        await sys.stop();
    }
}

test().catch(e => {
    console.error("Test Failed:", e.message);
    process.exit(1);
});
