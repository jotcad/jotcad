import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { VFS, MeshLink } from '../fs/src/index.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { launchSystem } from '../orchestrator.js';
import { registerFileProvider } from '../ux/src/lib/vfs/FileProvider.js';

// Disable TLS verification for self-signed certificates in local tests
process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const workspaceDir = path.resolve(__dirname, '..');
const libDir = path.resolve(workspaceDir, 'lib');

const filesToConvert = [
    { name: 'esp32-wemos-d1-mini', path: path.resolve(workspaceDir, 'integration', 'esp32-wemos-d1-mini.step') },
    { name: '28byj-48_5vdc', path: '/home/brian/Downloads/step/28BYJ-48_5VDC.step' },
    { name: 'uln2003_stepper_motor_driver_board', path: '/home/brian/Downloads/step/ULN2003_Stepper_Motor_Driver_Board.step' },
    { name: 'wemos-mini-d1', path: '/home/brian/Downloads/step/wemos-mini-d1.stp' }
];

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

async function consumeJSON(stream) {
    const bytes = await consumeBinary(stream);
    return JSON.parse(new TextDecoder().decode(bytes));
}

async function consumeText(stream) {
    const bytes = await consumeBinary(stream);
    return new TextDecoder().decode(bytes);
}

async function run() {
    // 1. Create lib directory if it doesn't exist
    if (!fs.existsSync(libDir)) {
        fs.mkdirSync(libDir, { recursive: true });
        console.log(`[Convert] Created directory: ${libDir}`);
    }

    console.log("[Convert] Launching local cluster...");
    const sys = await launchSystem('test/standard');

    // 2. Setup VFS and MeshLink
    const vfs = new VFS({ id: 'step-conversion-client' });
    const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
    
    await vfs.init();
    await mesh.start();
    registerFileProvider(vfs, mesh);

    // 3. Setup compiler & parser
    const compiler = new JotCompiler(vfs);
    const parser = new JotParser();

    // Register operators
    compiler.registerOperator('File', {
        path: 'jot/File',
        schema: {
            arguments: [{ name: 'path', type: 'jot:string' }],
            outputs: { "$out": { type: 'jot:file' } }
        }
    });

    compiler.registerOperator('Step', {
        path: 'jot/Step',
        schema: {
            arguments: [
                { name: 'file', type: 'jot:file' },
                { name: 'deflection', type: 'jot:number', default: 0.1 }
            ],
            outputs: { "$out": { type: 'jot:shape' } }
        }
    });

    try {
        for (const target of filesToConvert) {
            if (!fs.existsSync(target.path)) {
                console.warn(`[Convert] Skipping missing file: ${target.path}`);
                continue;
            }

            console.log(`\n========================================`);
            console.log(`[Convert] Processing: ${target.name}`);
            console.log(`[Convert] Source: ${target.path}`);
            console.log(`========================================`);

            // Use a large evaluation timeout (10 minutes) for heavy STEP files
            const context = { expiresAt: Date.now() + 600000 };
            const importCode = `Step(File("${target.path.replace(/\\/g, '/')}")) -> $out`;
            
            console.log(`[Convert] Evaluating DSL...`);
            const importAst = parser.parse(importCode);
            const importTerminals = await compiler.evaluate(importAst, {}, {
                outputs: { "$out": { type: "jot:shape" } }
            });
            
            const importBundle = importTerminals.find(t => t.selector.output === '$out');
            if (!importBundle) {
                throw new Error("Failed to find output bundle");
            }
            
            console.log("[Convert] Reading shape JSON from VFS...");
            const shapeResult = await vfs.readSelector(importBundle.selector, context);
            if (!shapeResult) {
                throw new Error("Shape entry not found in VFS");
            }
            
            const shape = await consumeJSON(shapeResult.stream);
            console.log(`[Convert] Shape loaded. Name: ${shape.tags.name}, Geometry CID: ${shape.geometry}`);

            // Write Shape JSON
            const shapePath = path.resolve(libDir, `${target.name}.json`);
            fs.writeFileSync(shapePath, JSON.stringify(shape, null, 2));
            console.log(`[Convert] Saved shape representation to: ${shapePath}`);

            if (shape.geometry) {
                console.log("[Convert] Reading geometry text from VFS...");
                const geoResult = await vfs.readCID(shape.geometry, context);
                if (!geoResult) {
                    throw new Error(`Geometry CID ${shape.geometry} not found in VFS`);
                }
                
                const geometryText = await consumeText(geoResult.stream);
                const geoPath = path.resolve(libDir, `${target.name}.geo`);
                fs.writeFileSync(geoPath, geometryText);
                console.log(`[Convert] Saved raw geometry mesh data to: ${geoPath}`);
            }

            console.log(`✔ Converted: ${target.name}`);
        }

        console.log("\n========================================");
        console.log("✔ All conversions completed!");
        console.log("========================================");

    } catch (err) {
        console.error("❌ Conversion FAILED:", err);
    } finally {
        console.log("[Convert] Shutting down VFS and cluster...");
        mesh.stop();
        await vfs.close();
        await sys.stop();
        console.log("[Convert] Exit.");
    }
}

run();
