import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { RESTBridge } from '../src/vfs_rest_bridge_node.js';
import { registerVFSRoutes } from '../src/vfs_rest_server.js';
import http from 'node:http';

test('Virtual Mailbox Protocol (Hub Routing)', async (t) => {
  // 1. Setup VFS Hub
  const hubVfs = new VFS({ id: 'hub' });
  const server = http.createServer();
  registerVFSRoutes(hubVfs, server, '/vfs');

  const port = await new Promise((resolve) =>
    server.listen(0, '127.0.0.1', () => resolve(server.address().port))
  );
  const baseUrl = `http://127.0.0.1:${port}/vfs`;

  // 2. Setup Provider Peer
  const providerVfs = new VFS({ id: 'provider' });
  const providerBridge = new RESTBridge(providerVfs, baseUrl);
  await providerBridge.start();

  // 3. Setup Requester Peer
  const requesterVfs = new VFS({ id: 'requester' });
  const requesterBridge = new RESTBridge(requesterVfs, baseUrl);
  await requesterBridge.start();

  await t.test('Identity Preservation & Demand Routing', { timeout: 10000 }, async () => {
    let callCount = 0;
    const testData = 'Hello from Provider Mailbox';

    // A. Provider announces LISTENING
    console.log('[Test] Provider announcing LISTENING for test/mailbox...');
    await providerVfs.receive({
        path: 'test/mailbox',
        parameters: {},
        state: 'LISTENING',
        source: `peer:${providerVfs.id}`
    });

    // Verify Hub saw it and preserved the source
    await new Promise(r => setTimeout(r, 500));
    const hubStates = await (await fetch(`${baseUrl}/states`)).json();
    const listeningState = hubStates.find(s => s.path === 'test/mailbox' && s.state === 'LISTENING');
    
    assert.ok(listeningState, 'Hub should have LISTENING state for test/mailbox');
    assert.ok(listeningState.sources.includes(`peer:${providerVfs.id}`), 'Hub should preserve peer: source identity in sources array');

    providerVfs.events.on('demand', async (d) => {
        if (d.path === 'test/mailbox') {
            console.log('[Test] Provider received demand for test/mailbox');
            callCount++;
            await providerVfs.writeData('test/mailbox', {}, testData);
        }
    });

    // C. Requester calls read()
    console.log('[Test] Requester calling read(test/mailbox)...');
    const result = await requesterVfs.readData('test/mailbox');

    assert.strictEqual(result, testData, 'Requester should receive data from provider via Hub routing');
    assert.strictEqual(callCount, 1, 'Provider mailbox should have been triggered exactly once');
  });

  await t.test('Nested Recursive Demand Routing', { timeout: 10000 }, async () => {
    // 1. Setup a "Transformer" peer that depends on "Provider"
    const transformerVfs = new VFS({ id: 'transformer' });
    const transformerBridge = new RESTBridge(transformerVfs, baseUrl);
    await transformerBridge.start();

    console.log('[Test] Transformer announcing LISTENING for op/transform...');
    await transformerVfs.receive({
        path: 'op/transform',
        parameters: {},
        state: 'LISTENING',
        source: `peer:${transformerVfs.id}`
    });

    // Wait for registration to propagate
    await new Promise(r => setTimeout(r, 500));

    // Transformer logic: read from provider, then wrap it
    transformerVfs.events.on('demand', async (d) => {
        if (d.path === 'op/transform') {
            console.log('[Test] Transformer received demand for op/transform, requesting from provider...');
            try {
                const innerData = await transformerVfs.readData('test/mailbox');
                console.log(`[Test] Transformer got data from provider: ${innerData}`);
                await transformerVfs.writeData('op/transform', {}, `Transformed(${innerData})`);
            } catch (err) {
                console.error('[Test] Transformer failed to read from provider:', err);
            }
        }
    });

    // 2. Requester calls the transformer
    console.log('[Test] Requester calling read(op/transform)...');
    const result = await requesterVfs.readData('op/transform');

    assert.strictEqual(result, 'Transformed(Hello from Provider Mailbox)', 'Should handle nested recursive routing');
    
    transformerBridge.stop();
    await transformerVfs.close();
  });

  // Cleanup
  await new Promise(r => setTimeout(r, 500));
  providerBridge.stop();
  requesterBridge.stop();
  server.close();
  await hubVfs.close();
  await providerVfs.close();
  await requesterVfs.close();
});
