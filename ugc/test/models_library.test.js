import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../../fs/src/index.js';
import { UGCEngine } from '../src/index.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('UGC Models Library: Automatic Model Loading & Fulfillment', async () => {
  const vfs = new VFS({ id: 'test-vfs', storage: new MemoryStorage() });
  await vfs.init();

  const ugc = new UGCEngine(vfs);
  await ugc.init();

  // 1. Verify that controllers/moga_thumbstick was registered
  const fulfillers = ugc.fulfiller.listFulfillers();
  const mogaFulfiller = fulfillers.find(f => f.path === 'user/controllers/moga_thumbstick');
  assert.ok(mogaFulfiller, 'user/controllers/moga_thumbstick should be registered in fulfiller list');
  assert.ok(mogaFulfiller.schema.arguments.find(a => a.name === 'cap_d'), 'Should have cap_d argument');

  const clamshellFulfiller = fulfillers.find(f => f.path === 'user/controllers/moga_hex_clamshell');
  assert.ok(clamshellFulfiller, 'user/controllers/moga_hex_clamshell should be registered in fulfiller list');
  assert.ok(clamshellFulfiller.schema.arguments.find(a => a.name === 'hex_d'), 'Should have hex_d argument');

  // 2. Evaluate a user script that invokes the model operator
  const script = 'controllers/moga_thumbstick(cap_d=16.5) -> $out;';
  const terminals = await ugc.evaluate(script);

  assert.strictEqual(terminals.length, 1);
  const sel = terminals[0].selector;
  assert.strictEqual(sel.path, 'user/controllers/moga_thumbstick');
  assert.strictEqual(sel.parameters.cap_d, 16.5);
  assert.strictEqual(sel.parameters.dome_d, 18.0); // Default preserved
});
