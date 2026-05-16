import { VFS, MeshLink, Selector } from './fs/src/index.js';
import { JotCompiler } from './jot/src/compiler.js';
import { JotParser } from './jot/src/parser.js';
import fs from 'fs';

async function test() {
    // 1. Setup VFS with MeshLink to the native ops node
    const vfs = new VFS({ id: 'test-client' });
    const mesh = new MeshLink(vfs, ['http://localhost:9091']);
    
    await vfs.init();
    await mesh.start();

    // 2. Setup Jot Compiler
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();
    
    // Register STL and Box for the compiler (defines schema/outputs)
    compiler.registerOperator('stl', {
        path: 'jot/stl',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape' },
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

    const code = 'Box(10).stl("test.stl")';
    console.log("Parsing & Evaluating:", code);
    const ast = parser.parse(code);
    const terminals = await compiler.evaluate(ast);
    
    const stlBundle = terminals.find(t => t.selector.output === '$out');
    const stlTerminal = stlBundle?.selector;
    if (!stlTerminal) {
        console.error("Failed to find STL file terminal");
        process.exit(1);
    }

    // 3. Use standard VFS readData to fetch the STL bytes via mesh
    console.log("Requesting STL bytes via vfs.readData()...");
    const bytes = await vfs.readData(stlTerminal);
    
    if (!bytes) {
        console.error("Failed to read STL bytes from mesh");
        process.exit(1);
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

    mesh.stop();
    await vfs.close();
}

test().catch(async (e) => {
    console.error("Test Failed:", e.message);
    process.exit(1);
});
