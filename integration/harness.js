import test from 'node:test';
import { VFS, MeshLink, Selector } from '../fs/src/index.js';
import { launchSystem } from '../orchestrator.js';
import { JotCompiler } from '../jot/src/compiler.js';
import { JotParser } from '../jot/src/parser.js';
import { waitForMeshNodes } from './vfs_test_helpers.js';
import { captureAndVerifyPNG } from './png_helper.js';

/**
 * Runs an integration test with a standard test system, VFS node, mesh peerness,
 * and Jot Compiler initialized with all schemas from the mesh catalog.
 * 
 * Can be called with either a callback function:
 *   runIntegrationTest(name, async ({ compiler, parser, readData, capturePNG }) => { ... })
 * Or declaratively:
 *   runIntegrationTest(name, { script, png, pngOptions })
 * 
 * @param {string} testName 
 * @param {Function|Object} optionsOrFn 
 */
export async function runIntegrationTest(testName, optionsOrFn) {
    test(testName, { timeout: 120000 }, async (t) => {
        let sys = null;
        let vfs = null;
        let mesh = null;
        try {
            // 1. Launch standard system
            console.log(`[Harness] Launching test cluster for '${testName}'...`);
            sys = await launchSystem('test/standard');

            // 2. Start local VFS Node and MeshLink
            const nodeId = `${testName.replace(/[^a-zA-Z0-9]+/g, '-').toLowerCase()}-node`;
            vfs = new VFS({ id: nodeId });
            mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
            
            await vfs.init();
            await mesh.start();

            // 3. Wait for mesh to sync and find operators
            await waitForMeshNodes(vfs, ['geo-ops-node']);

            // 4. Fetch the catalog dynamically to compile correctly
            let catalogReceived = null;
            vfs.events.on('notify', (selector, payload) => {
                if (selector.path === 'sys/schema') {
                    catalogReceived = payload;
                }
            });
            await mesh.subscribe(new Selector('sys/schema'), Date.now() + 15000);

            let attempts = 0;
            while (!catalogReceived && attempts < 50) {
                await new Promise(r => setTimeout(r, 100));
                attempts++;
            }
            if (!catalogReceived) {
                throw new Error("Failed to receive schema catalog from mesh");
            }

            // 5. Setup JotCompiler and JotParser
            const compiler = new JotCompiler(vfs);
            const parser = new JotParser();
            
            for (const [path, schema] of Object.entries(catalogReceived.catalog)) {
                const shortName = path.split('/').pop();
                compiler.registerOperator(shortName, { path, schema });
            }

            if (typeof optionsOrFn === 'function') {
                const readData = async (selector) => {
                    const result = await vfs.readSelector(selector);
                    return vfs._drainStream(result);
                };
                const capturePNG = async (selector, filename, opts = {}) => {
                    return captureAndVerifyPNG(vfs, selector, filename, null, opts);
                };
                await optionsOrFn({ t, vfs, compiler, parser, readData, capturePNG });
            } else {
                // Declarative execution
                const { script, png, pngOptions = {} } = optionsOrFn;
                console.log(`[Harness] Compiling declarative JOT script...`);
                const ast = parser.parse(script.trim());
                const results = await compiler.evaluate(ast, {}, { outputs: { $out: { type: 'jot:shape' } } });
                if (results.length === 0) {
                    throw new Error("Declarative JOT script returned no output bundles");
                }
                if (png) {
                    console.log(`[Harness] Capturing declarative PNG snapshot: ${png}`);
                    await captureAndVerifyPNG(vfs, results[0].selector, png, null, pngOptions);
                }
            }

        } finally {
            console.log(`[Harness] Cleaning up test '${testName}'...`);
            if (mesh) await mesh.stop();
            if (vfs) await vfs.close();
            if (sys) await sys.stop();
        }
    });
}
