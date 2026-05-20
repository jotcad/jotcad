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
        inputs: { '$in': { type: 'jot:shape' } },
        arguments: [{ name: 'angle', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  await t.test('Should resolve PascalCase Constructors', async () => {
    const schema = { outputs: { "$out": { type: "jot:shape" } } };
    const res = await compiler.evaluate({ 
        type: 'ASSIGNMENT', 
        name: '$out', 
        value: { type: 'CALL', name: 'Box', args: [10] }
    }, {}, schema);
    const resolved = res[0].selector;
    assert.strictEqual(resolved.path, 'shape/box');
  });

  await t.test('Should resolve camelCase Operations', async () => {
    const schema = { outputs: { "$out": { type: "jot:shape" } } };
    const res = await compiler.evaluate({ 
        type: 'ASSIGNMENT', 
        name: '$out', 
        value: { 
            type: 'METHOD', 
            subject: { path: 'shape/box', parameters: {} }, 
            name: 'rotateX', 
            args: [45] 
        }
    }, {}, schema);
    const resolved = res[0].selector;
    assert.strictEqual(resolved.path, 'op/rotateX');
  });

  await t.test('Should NOT resolve if casing is wrong', async () => {
    await assert.rejects(async () => {
      const schema = { outputs: { "$out": { type: "jot:shape" } } };
      await compiler.evaluate({ 
        type: 'ASSIGNMENT', 
        name: '$out', 
        value: { type: 'CALL', name: 'box', args: [10] }
      }, {}, schema);
    }, /Unregistered operator/);
  });
});
