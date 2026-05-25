import test from 'node:test';
import assert from 'node:assert';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { log } from '../fs/src/log.js';

test('Orchestrator Lifecycle: Cluster Launch and Shutdown', async (t) => {
    log('[Test Orchestrator] Starting cluster lifecycle test...');
    
    // 1. Launch the TEST cluster
    const sys = await launchSystem(PROFILES.TEST);
    const { ports } = sys;

    try {
        await t.test('Ops Node should be healthy', async () => {
            const resp = await fetch(`http://localhost:${ports.ops}/health`);
            assert.ok(resp.ok, 'Ops Node health check failed');
            const data = await resp.json();
            assert.strictEqual(data.status, 'OK');
        });

        await t.test('Export Node should be healthy', async () => {
            // Note: PROFILES.TEST starts Export node in HTTPS if certs exist
            // Orchestrator handles protocol detection in its own health checks,
            // but for this test we'll probe based on the profile assumption.
            const protocol = 'https'; // Export Node defaults to HTTPS in TEST profile if certs found
            const resp = await fetch(`${protocol}://localhost:${ports.export}/health`, {
                // Ignore self-signed cert issues for the test
                dispatcher: new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } })
            });
            assert.ok(resp.ok, 'Export Node health check failed');
        });

        await t.test('UX should be reachable', async () => {
            const { isHttps } = sys;
            const protocol = isHttps ? 'https' : 'http';
            const options = {};
            if (isHttps) {
                options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
            }
            const resp = await fetch(`${protocol}://localhost:${ports.ux}/`, options);
            assert.ok(resp.ok, 'UX reachability check failed');
        });

    } finally {
        log('[Test Orchestrator] Shutting down cluster...');
        await sys.stop();
    }
});
