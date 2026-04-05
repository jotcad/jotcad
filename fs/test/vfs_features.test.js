import { test } from 'node:test';
import assert from 'node:assert';
import { VFS } from '../src/vfs_node.js';
import { Readable } from 'stream';

test('VFS Extended Features', async (t) => {
  await t.test('declare() sets SCHEMA state and propagates data', async () => {
    const vfs = new VFS({ id: 'test-declare' });
    const schema = { type: 'object', properties: { size: { type: 'number' } } };
    
    await vfs.declare('test/shape', schema);
    
    const status = await vfs.status('test/shape@schema');
    assert.strictEqual(status, 'SCHEMA');
    
    // readData should automatically parse it back
    const data = await vfs.readData('test/shape@schema');
    assert.deepStrictEqual(data, schema);
    await vfs.close();
  });

  await t.test('writeData() and readData() handle JSON automatically', async () => {
    const vfs = new VFS({ id: 'test-json' });
    const complexData = { foo: 'bar', nums: [1, 2, 3] };
    
    await vfs.writeData('data/json', {}, complexData);
    
    // State should be AVAILABLE
    assert.strictEqual(await vfs.status('data/json'), 'AVAILABLE');
    
    const readBack = await vfs.readData('data/json');
    assert.deepStrictEqual(readBack, complexData);
    await vfs.close();
  });

  await t.test('read() deduplication prevents multiple listeners', async () => {
    const vfs = new VFS({ id: 'test-dedup' });
    
    // Start multiple concurrent reads
    const p1 = vfs.read('pending/item');
    const p2 = vfs.read('pending/item');
    
    // Give the async getCID a chance to resolve and register the active read
    await new Promise(r => setTimeout(r, 10));
    
    const cid = await vfs.getCID({ path: 'pending/item', parameters: {} });
    assert.ok(vfs.activeReads.has(cid), 'Should track active read via CID');
    
    await vfs.writeData('pending/item', {}, "fulfilled");
    
    const [s1, s2] = await Promise.all([p1, p2]);
    assert.ok(s1 && s2, 'All promises should resolve');
    assert.ok(!vfs.activeReads.has(cid), 'Active read should be cleared');
    
    await vfs.close();
  });

  await t.test('receive() robustly handles numeric-key objects', async () => {
    const vfs = new VFS({ id: 'test-robust' });
    const path = 'robust/test';
    const cid = await vfs.getCID({ path, parameters: {} });
    
    const simulatedEvent = {
        cid,
        path,
        parameters: {},
        state: 'AVAILABLE',
        data: { "0": 72, "1": 101, "2": 108, "3": 108, "4": 111 } // "Hello"
    };
    
    await vfs.receive(simulatedEvent);
    const data = await vfs.readData(path);
    assert.strictEqual(data, 'Hello');
    await vfs.close();
  });
});
