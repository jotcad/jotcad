import assert from 'node:assert';
import fs from 'node:fs';
import { Selector } from '../fs/src/index.js';
import { runIntegrationTest } from './harness.js';

runIntegrationTest('Orchestrator Lifecycle: Cluster Launch and Shutdown', async ({ t, vfs, sys, readData, catalog }) => {
    await t.test('Ops Node should be healthy', async () => {
        assert.ok(catalog, 'Should have received schema catalog');
        assert.ok(catalog.catalog, 'Catalog payload should contain catalog object');
    });

    await t.test('Export Node should be healthy', async () => {
        const ports = sys.ports;
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
        const ports = sys.ports;
        const hasCerts = fs.existsSync('.ssl/localhost-key.pem') && fs.existsSync('.ssl/localhost-cert.pem');
        const protocol = hasCerts ? 'https' : 'http';
        const options = {};
        if (protocol === 'https') {
            options.dispatcher = new (await import('undici')).Agent({ connect: { rejectUnauthorized: false } });
        }
        const resp = await fetch(`${protocol}://localhost:${ports.ux}/`, options);
        assert.ok(resp.ok || resp.status === 404, 'UX reachability check failed');
    });
});
