import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Operator Variant Resolution (Strict Casing)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // Constructor (PascalCase)
  compiler.registerOperator('Box', {
    path: 'shape/box',
    schema: { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { "$out": { type: "jot:shape" } } }
  });

  // Operation (camelCase)
  compiler.registerOperator('rotateX', {
    path: 'op/rotateX',
    schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'angle', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  await t.test('Should resolve PascalCase Constructors', async () => {
    const res = await compiler.evaluate({ type: 'CALL', name: 'Box', args: [10] });
    const resolved = res[0];
    assert.strictEqual(resolved.path, 'shape/box');
  });

  await t.test('Should resolve camelCase Operations', async () => {
    const res = await compiler.evaluate({ 
        type: 'METHOD', 
        subject: { path: 'shape/box', parameters: {} }, 
        name: 'rotateX', 
        args: [45] 
    });
    const resolved = res[0];
    assert.strictEqual(resolved.path, 'op/rotateX');
  });

  await t.test('Should NOT resolve if casing is wrong', async () => {
    await assert.rejects(async () => {
      await compiler.evaluate({ type: 'CALL', name: 'box', args: [10] });
    }, /Unregistered operator/);
  });
});
