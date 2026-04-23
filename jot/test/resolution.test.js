import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '@jotcad/fs';
import { JotCompiler } from '../src/compiler.js';

test('Operator Variant Resolution (Strict Casing)', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });

  await t.test('Should resolve PascalCase Constructors', async () => {
    const compiler = new JotCompiler(vfs);
    compiler.registerOperator('Hexagon/full', { 
        path: 'jot/Hexagon/full',
        schema: { arguments: { diameter: { type: 'number' } } }
    });
    compiler.registerOperator('Hexagon/cap', { 
        path: 'jot/Hexagon/cap',
        schema: { arguments: { diameter: { type: 'number' }, type: { type: 'string', const: 'cap' } } }
    });

    const op = await compiler._evaluateCall({ name: 'Hexagon', args: [30] }, {});
    assert.strictEqual(op.path, 'jot/Hexagon/full');
  });

  await t.test('Should resolve camelCase Operations', async () => {
    const compiler = new JotCompiler(vfs);
    compiler.registerOperator('offset/base', { 
        path: 'jot/offset',
        schema: { arguments: { diameter: { type: 'number' } } }
    });
    compiler.registerOperator('offset/closure', { 
        path: 'jot/offset/closure',
        schema: { arguments: { diameter: { type: 'number' }, closure: { type: 'boolean', const: true } } }
    });

    // .offset(5)
    const op1 = await compiler._evaluateMethod({ name: 'offset', args: [5], subject: { path: 'jot/Box', parameters: {} } }, {});
    assert.strictEqual(op1.path, 'jot/offset');

    // .offset(5, closure: true)
    const op2 = await compiler._evaluateMethod({ name: 'offset', args: [5, { __annotated: true, nameHint: 'closure', value: true }], subject: { path: 'jot/Box', parameters: {} } }, {});
    assert.strictEqual(op2.path, 'jot/offset/closure');
  });

  await t.test('Should NOT resolve if casing is wrong', async () => {
    const compiler = new JotCompiler(vfs);
    compiler.registerOperator('Hexagon/full', { path: 'jot/Hexagon/full', schema: {} });
    
    // hexagon(30) should fail because it's not PascalCase
    await assert.rejects(() => compiler._evaluateCall({ name: 'hexagon', args: [30] }, {}), /Unregistered/);
  });
});
