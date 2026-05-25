import test from 'node:test';
import assert from 'node:assert';
import { launchSystem, PROFILES } from '../orchestrator.js';
import { log } from '../fs/src/log.js';

test('Orchestrator Lifecycle: Cluster Launch and Shutdown', async (t) => {
    log('[Test Orchestrator] Starting cluster lifecycle test...');
    
    // 1. Launch the TEST cluster
    const profileKey = 'test/standard';
    const sys = await launchSystem(profileKey);
    const { ports } = sys;

    try {
        await t.test('Ops Node should be healthy', async () => {
            const resp = await fetch(`http://localhost:${ports.ops}/health`);
            assert.ok(resp.ok, 'Ops Node health check failed');
            const data = await resp.json();
            assert.strictEqual(data.status, 'OK');
        });

        await t.test('Export Node should be healthy', async () => {
            const resp = await fetch(`https://localhost:${ports.export}/health`, {
                // Ignore self-signed cert issues for the test
                dispatcher: new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } })
            });
            assert.ok(resp.ok, 'Export Node health check failed');
        });

        await t.test('UX should be reachable', async () => {
            const resp = await fetch(`https://localhost:${ports.ux}/`, {
                dispatcher: new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } })
            });
            assert.ok(resp.ok, 'UX reachability check failed');
        });

    } finally {
        log('[Test Orchestrator] Shutting down cluster...');
        await sys.stop();
    }
});
