import test from 'node:test';
import assert from 'node:assert';
import { VFS, Selector, getSelectorKey } from '../fs/src/index.js';
import { MeshLink } from '../fs/src/mesh_link.js';
import { launchSystem } from '../orchestrator.js';

test('Mesh Pub-Sub via Zenoh (ez pubsub)', { timeout: 60000 }, async (t) => {
    // 1. Launch the TEST system
    const sys = await launchSystem('test/standard');
    const ROUTER_PORT = sys.ports.zenoh_router;
    const routerUrl = `http://localhost:${ROUTER_PORT}`;

    // 2. Initialize VFS nodes and MeshLinks
    const vfsSub = new VFS({ id: 'node-js-sub' });
    const vfsPub = new VFS({ id: 'node-js-pub' });

    await vfsSub.init();
    await vfsPub.init();

    const meshSub = new MeshLink(vfsSub, [routerUrl]);
    const meshPub = new MeshLink(vfsPub, [routerUrl]);

    try {
        console.log('[Test] Starting MeshLinks connected to Zenoh router...');
        await meshSub.start();
        await meshPub.start();

        // 3. Test JSON notification propagation
        console.log('[Test] Testing JSON notification...');
        const topicJson = new Selector('test/ez-json-topic');
        let receivedJson = null;

        vfsSub.events.on('notify', (selector, payload) => {
            if (selector.path === topicJson.path) {
                receivedJson = payload;
            }
        });

        await vfsSub.subscribe(topicJson);
        await new Promise(r => setTimeout(r, 500)); // Allow subscription to propagate

        const jsonPayload = { greeting: 'Hello via Zenoh!', num: 42 };
        meshPub.notify(topicJson, jsonPayload);

        let attempts = 0;
        while (!receivedJson && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(receivedJson, 'Subscriber should have received JSON notification');
        assert.deepStrictEqual(receivedJson, jsonPayload);
        console.log('✔ JSON notification received successfully');

        // 4. Test Binary notification propagation
        console.log('[Test] Testing Binary notification...');
        const topicBin = new Selector('test/ez-bin-topic');
        let receivedBin = null;

        vfsSub.events.on('notify', (selector, payload) => {
            if (selector.path === topicBin.path) {
                receivedBin = payload;
            }
        });

        await vfsSub.subscribe(topicBin);
        await new Promise(r => setTimeout(r, 500));

        const binaryData = new Uint8Array([0xAA, 0xBB, 0xCC, 0xDD, 0xEE]);
        meshPub.notify(topicBin, binaryData);

        attempts = 0;
        while (!receivedBin && attempts < 50) {
            await new Promise(r => setTimeout(r, 100));
            attempts++;
        }

        assert.ok(receivedBin, 'Subscriber should have received binary notification');
        assert.ok(receivedBin instanceof Uint8Array, 'Received payload must be a Uint8Array');
        assert.deepStrictEqual(Array.from(receivedBin), Array.from(binaryData));
        console.log('✔ Binary notification received successfully');

        // 5. Test Operator Load-Shedding with active CID Caching
        console.log('[Test] Testing Operator Load-Shedding with active CID Caching...');
        let callCount = 0;
        vfsPub.registerProvider('test/load-shedding-op', async () => {
            callCount++;
            return {
                stream: new ReadableStream({
                    start(controller) {
                        controller.enqueue(new TextEncoder().encode(JSON.stringify({ calculated: true, count: callCount })));
                        controller.close();
                    }
                }),
                metadata: { state: 'AVAILABLE', encoding: 'json' }
            };
        });

        const selector = new Selector('test/load-shedding-op');
        const targetCID = await getSelectorKey(selector);

        const pathStr = 'test/load-shedding-op';

        // Initial read by Client (vfsSub) - triggers calculation on Worker (vfsPub)
        const initialResult = await vfsSub.read(pathStr);
        assert.deepStrictEqual(initialResult, { calculated: true, count: 1 });
        assert.strictEqual(callCount, 1);
        console.log('✔ Initial calculation succeeded');

        // Simulate load-shedding on Worker:
        // Worker undeclares its operator queryable, but keeps the CID queryable active
        const opQueryable = meshPub.queryableOps.get('test/load-shedding-op');
        assert.ok(opQueryable, 'Operator queryable should be registered');
        await opQueryable.undeclare();
        meshPub.queryableOps.delete('test/load-shedding-op');
        console.log('✔ Operator queryable undeclared (simulating load shedding)');

        // Client requests the exact same selector again.
        // The query should succeed immediately because the CID queryable remains active on the Worker,
        // allowing the client to resolve it from the Worker's cache without re-computation!
        const cachedResult = await vfsSub.read(pathStr);
        assert.deepStrictEqual(cachedResult, { calculated: true, count: 1 });
        assert.strictEqual(callCount, 1); // callCount remains 1 (no re-computation)
        console.log('✔ Cached CID query resolved successfully despite operator load shedding');

        // Client requests a new selector (with different params) which is not cached
        const newSelector = new Selector('test/load-shedding-op', { salt: 'new' });
        try {
            await vfsSub.read(newSelector, { expiresAt: Date.now() + 2000 });
            assert.fail('Should have failed because the operator queryable is undeclared');
        } catch (err) {
            assert.ok(err.message, 'Fails to resolve new selector as expected');
            console.log('✔ New selector query failed as expected (operator is inactive)');
        }

    } finally {
        await meshSub.stop();
        await meshPub.stop();
        await vfsSub.close();
        await vfsPub.close();
        await sys.stop();
    }
});
