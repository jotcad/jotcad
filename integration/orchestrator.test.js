import test from 'node:test';
import assert from 'node:assert';
import fs from 'node:fs';
import { launchSystem } from '../orchestrator.js';
import { log } from '../fs/src/log.js';

test('Orchestrator Lifecycle: Cluster Launch and Shutdown', async (t) => {
    log('[Test Orchestrator] Starting cluster lifecycle test...');
    
    // 1. Launch the TEST cluster
    const profileKey = 'test/standard';
    const sys = await launchSystem(profileKey);
    const { ports } = sys;

    try {
        await t.test('Ops Node should be healthy', async () => {
            const { VFS, MeshLink, Selector } = await import('../fs/src/index.js');
            const { waitForMeshNodes } = await import('./vfs_test_helpers.js');
            const vfs = new VFS({ id: 'orchestrator-test-client' });
            const mesh = new MeshLink(vfs, [`http://localhost:${sys.ports.zenoh_router}`]);
            await vfs.init();
            await mesh.start();

            // Wait for C++ Ops Node to be ready on the Zenoh router
            await waitForMeshNodes(vfs, ['geo-ops-node']);

            try {
                let catalogReceived = null;
                vfs.events.on('notify', (selector, payload) => {
                    if (selector.path === 'sys/schema') {
                        catalogReceived = payload;
                    }
                });

                await mesh.subscribe(Selector.fromObject({ path: 'sys/schema' }), Date.now() + 10000);

                let attempts = 0;
                while (!catalogReceived && attempts < 50) {
                    await new Promise(r => setTimeout(r, 100));
                    attempts++;
                }

                assert.ok(catalogReceived, 'Should have received schema catalog');
                assert.ok(catalogReceived.catalog, 'Catalog payload should contain catalog object');
            } finally {
                mesh.stop();
                await vfs.close();
            }
        });

        await t.test('Export Node should be healthy', async () => {
            const hasCerts = fs.existsSync('.ssl/localhost-key.pem') && fs.existsSync('.ssl/localhost-cert.pem');
            const protocol = hasCerts ? 'https' : 'http';
            const options = {};
            if (protocol === 'https') {
                options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
            }
            const resp = await fetch(`${protocol}://localhost:${ports.export}/health`, options);
            assert.ok(resp.ok, 'Export Node health check failed');
            const data = await resp.json();
            assert.strictEqual(data.status, 'OK');
        });

        await t.test('UX should be reachable', async () => {
            const hasCerts = fs.existsSync('.ssl/localhost-key.pem') && fs.existsSync('.ssl/localhost-cert.pem');
            const protocol = hasCerts ? 'https' : 'http';
            const options = {};
            if (protocol === 'https') {
                options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
            }
            const resp = await fetch(`${protocol}://localhost:${ports.ux}/`, options);
            assert.ok(resp.ok || resp.status === 404, 'UX reachability check failed');
        });

    } finally {
        log('[Test Orchestrator] Shutting down cluster...');
        await sys.stop();
    }
});
