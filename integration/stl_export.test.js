import { VFS, MeshLink, Selector } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem, PROFILES } from '../orchestrator.js';
import fs from 'fs';

async function consumeBinary(stream) {
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
    return bytes;
}

async function test() {
    console.log("Starting STL Export Integration Test...");
    
    // 1. Launch the TEST system (ops node on 9191)
    const sys = await launchSystem('test/standard');
    const opsUrl = `http://localhost:${sys.ports.ops}`;

    // 2. Setup VFS and MeshLink
    const vfs = new VFS({ id: 'test-client' });
    const mesh = new MeshLink(vfs, [opsUrl]);
    
    await vfs.init();
    await mesh.start();

    // 3. Setup Jot Compiler
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();
    
    // Register STL and Box for the compiler (defines schema/outputs)
    compiler.registerOperator('stl', {
        path: 'jot/stl',
        schema: {
            inputs: { '$in': { type: 'jot:shape' } },
            arguments: [
                { name: 'path', type: 'jot:string', default: 'export.stl' }
            ],
            outputs: {
                "$out": { type: 'file' }
            }
        }
    });

    compiler.registerOperator('Box', {
        path: 'jot/Box',
        schema: {
            arguments: [{ name: 'width', type: 'jot:interval' }],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    try {
        const code = 'Box(10).stl("test.stl") -> $out';
        console.log("Parsing & Evaluating:", code);
        const ast = parser.parse(code);
        const terminals = await compiler.evaluate(ast, {}, {
            outputs: { "$out": { type: "file" } }
        });
        
        const stlBundle = terminals.find(t => t.selector.output === '$out');
        const stlTerminal = stlBundle?.selector;
        if (!stlTerminal) {
            throw new Error("Failed to find STL file terminal");
        }

        // 4. Use standard VFS read to fetch the STL bytes via mesh
        console.log("Requesting STL bytes via vfs.read()...");
        const result = await vfs.read(stlTerminal);
        if (!result) {
            throw new Error("Failed to read STL from mesh");
        }
        const bytes = await consumeBinary(result.stream);
        
        if (!bytes) {
            throw new Error("Failed to consume STL bytes");
        }

        console.log("STL File Size:", bytes.byteLength);
        
        // Binary STL: 80 (header) + 4 (count) + 12 (triangles) * 50 = 684 bytes
        if (bytes.byteLength === 684) {
            console.log("SUCCESS: STL file size matches expectations (684 bytes).");
        } else {
            console.warn("WARNING: STL file size mismatch. Expected 684, got", bytes.byteLength);
        }

        fs.writeFileSync('test.stl', Buffer.from(bytes));
        console.log("Wrote test.stl");
    } finally {
        console.log("Cleaning up...");
        mesh.stop();
        await vfs.close();
        await sys.stop();
    }
}

test().catch(async (e) => {
    console.error("Test Failed:", e.message);
    process.exit(1);
});
