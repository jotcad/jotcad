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
 *   runIntegrationTest(name, async ({ compiler, parser, readData, capturePNG, createNode, sys, mesh, vfs }) => { ... })
 * Or declaratively:
 *   runIntegrationTest(name, { script, png, pngOptions })
 * 
 * @param {string} testName 
 * @param {Function|Object} optionsOrFn 
 */
export async function runIntegrationTest(testName, optionsOrFn) {
    test(testName, { timeout: 120000 }, async (t) => {
        let sys = null;
        const activeNodes = [];

        // Factory helper to spawn additional nodes that will be automatically cleaned up
        const createNode = async (id, vfsConfig = {}) => {
            const nodeVfs = new VFS({ id, ...vfsConfig });
            const nodeMesh = new MeshLink(nodeVfs, [`http://localhost:${sys.ports.zenoh_router}`]);
            await nodeVfs.init();
            await nodeMesh.start();
            activeNodes.push({ vfs: nodeVfs, mesh: nodeMesh });

            const compiler = new JotCompiler(nodeVfs);
            const parser = new JotParser();
            
            return { vfs: nodeVfs, mesh: nodeMesh, compiler, parser };
        };

        try {
            // 1. Launch standard system
            console.log(`[Harness] Launching test cluster for '${testName}'...`);
            sys = await launchSystem('test/standard');

            if (typeof optionsOrFn === 'function') {
                // Procedural callback test
                const nodeId = `${testName.replace(/[^a-zA-Z0-9]+/g, '-').toLowerCase()}-node`;
                const { vfs, mesh, compiler, parser } = await createNode(nodeId);

                // Wait for mesh to sync and find operators
                await waitForMeshNodes(vfs, ['geo-ops-node']);

                // Fetch catalog
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
                if (catalogReceived) {
                    for (const [path, schema] of Object.entries(catalogReceived.catalog)) {
                        const shortName = path.split('/').pop();
                        compiler.registerOperator(shortName, { path, schema });
                    }
                }

                const readData = async (selector) => {
                    const result = await vfs.readSelector(selector);
                    return vfs._drainStream(result);
                };
                const capturePNG = async (selector, filename, opts = {}) => {
                    return captureAndVerifyPNG(vfs, selector, filename, null, opts);
                };

                await optionsOrFn({
                    t,
                    vfs,
                    mesh,
                    sys,
                    compiler,
                    parser,
                    readData,
                    capturePNG,
                    createNode
                });

            } else {
                // Declarative JOT script test
                const { script, png, pngOptions = {} } = optionsOrFn;
                const nodeId = `${testName.replace(/[^a-zA-Z0-9]+/g, '-').toLowerCase()}-node`;
                const { vfs, compiler, parser } = await createNode(nodeId);

                await waitForMeshNodes(vfs, ['geo-ops-node']);

                // Fetch catalog
                let catalogReceived = null;
                vfs.events.on('notify', (selector, payload) => {
                    if (selector.path === 'sys/schema') {
                        catalogReceived = payload;
                    }
                });
                await vfs.mesh.subscribe(new Selector('sys/schema'), Date.now() + 15000);
                
                let attempts = 0;
                while (!catalogReceived && attempts < 50) {
                    await new Promise(r => setTimeout(r, 100));
                    attempts++;
                }
                if (!catalogReceived) {
                    throw new Error("Failed to receive schema catalog from mesh");
                }

                for (const [path, schema] of Object.entries(catalogReceived.catalog)) {
                    const shortName = path.split('/').pop();
                    compiler.registerOperator(shortName, { path, schema });
                }

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
            console.log(`[Harness] Cleaning up active nodes and cluster...`);
            for (const node of activeNodes) {
                try {
                    await node.mesh.stop();
                    await node.vfs.close();
                } catch (e) {
                    console.error("[Harness] Error tearing down node:", e.message);
                }
            }
            if (sys) {
                await sys.stop();
            }
        }
    });
}
