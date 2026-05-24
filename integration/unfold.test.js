import { VFS, MeshLink } from '../fs/src/index.js';
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

function parseGeometry(text) {
    const lines = text.split('\n');
    const vertices = [];
    let i = 0;
    while (i < lines.length) {
        const line = lines[i].trim();
        if (line.startsWith('V ')) {
            const count = parseInt(line.substring(2).trim(), 10);
            i++;
            for (let c = 0; c < count && i < lines.length; c++) {
                const parts = lines[i].trim().split(/\s+/);
                if (parts.length >= 3) {
                    const parseCoord = (s) => {
                        if (s.includes('/')) {
                            const [num, den] = s.split('/');
                            return parseFloat(num) / parseFloat(den);
                        }
                        return parseFloat(s);
                    };
                    vertices.push({
                        x: parseCoord(parts[0]),
                        y: parseCoord(parts[1]),
                        z: parseCoord(parts[2])
                    });
                }
                i++;
            }
        } else {
            i++;
        }
    }
    return { vertices };
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
    
    // 3. Load all catalog operators from the ops server
    const catalogUrl = `${opsUrl}/catalog`;
    console.log("Fetching catalog from:", catalogUrl);
    const resp = await fetch(catalogUrl);
    if (!resp.ok) {
        throw new Error(`Catalog fetch failed: ${resp.status}`);
    }
    const catalog = await resp.json();
    for (const [name, schema] of Object.entries(catalog.catalog)) {
        compiler.registerOperator(name, { path: name, schema: schema });
    }

    try {
        // --- Test 1: Box Unfold ---
        const code1 = "Box(10, 10, 10).color('red').unfold() -> $out";
        console.log("Evaluating Test 1:", code1);
        const ast1 = parser.parse(code1);
        
        const terminals1 = await compiler.evaluate(ast1, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const unfoldResult1 = terminals1.find(t => t.port === '$out');
        const shape1 = await consumeJSON((await vfs.read(unfoldResult1.selector)).stream);
        
        console.log("Box Unfold - Number of islands:", (shape1.components || []).length);
        await captureAndVerifyPNG(vfs, unfoldResult1.selector, 'unfold_box_result.png', '091de468804708b16ade93cddd046d9ad7aa325a91dd04caac0ed101cb549531');

        if (!shape1.components || shape1.components.length !== 1) {
            throw new Error(`Expected 1 island for a cube net, got ${shape1.components ? shape1.components.length : 0}`);
        }

        // --- Test 2: Orb Unfold ---
        const code2 = "Orb(1).color('blue').unfold().pack(sheet=Box(4, 4).color('grey')) -> $out";
        console.log("\nEvaluating Test 2:", code2);
        const ast2 = parser.parse(code2);
        
        const terminals2 = await compiler.evaluate(ast2, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const unfoldResult2 = terminals2.find(t => t.port === '$out');
        const shape2 = await consumeJSON((await vfs.read(unfoldResult2.selector)).stream);
        
        console.log("Packed Orb Unfold - Bins:", (shape2.components || []).length);
        
        // We don't have a hash for the packed orb yet, so we just capture it
        await captureAndVerifyPNG(vfs, unfoldResult2.selector, 'unfold_orb_result.png');

        // --- Test 3: Hollow Box Unfold (Regression Test) ---
        const code3 = "U = Box(20, 20, 20).cut(Box(10, 10, 20)).unfold(strategy=\"pair\"); Cuts = U.inItem(\"*\").has(\"unfold\", \"cut\").color('red'); Folds = U.inItem(\"*\").has(\"unfold\", \"fold\").color('green'); Folds.and(Cuts).pack(sheet=Box(100, 100)) -> $out";
        console.log("\nEvaluating Test 3:", code3);
        const ast3 = parser.parse(code3);
        
        const terminals3 = await compiler.evaluate(ast3, {}, {
            outputs: { "$out": { type: "jot:shape" } }
        });
        
        const unfoldResult3 = terminals3.find(t => t.port === '$out');
        const shape3 = await consumeJSON((await vfs.read(unfoldResult3.selector)).stream);
        
        console.log("Hollow Box Unfold & Pack - Components:", (shape3.components || []).length);
        await captureAndVerifyPNG(vfs, unfoldResult3.selector, 'unfold_hollow_box_result.png');

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
