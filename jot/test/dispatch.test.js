import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '@jotcad/fs';
import { JotCompiler } from '../src/compiler.js';

test('Operator Variant Resolution and Dispatch', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });
  const compiler = new JotCompiler(vfs);

  // --- Triangle Variants ---
  compiler.registerOperator('Triangle/equilateral', { 
    path: 'jot/Triangle/equilateral', 
    schema: { arguments: { base: { type: 'number' } } } 
  });
  compiler.registerOperator('Triangle/right', { 
    path: 'jot/Triangle/right', 
    schema: { arguments: { base: { type: 'number' }, height: { type: 'number' } } } 
  });

  // --- Offset Variants ---
  compiler.registerOperator('offset', { 
    path: 'jot/offset', 
    schema: { arguments: { diameter: { type: 'number' } } } 
  });
  compiler.registerOperator('offset/closure', { 
    path: 'jot/offset/closure', 
    schema: { arguments: { diameter: { type: 'number' } } } 
  });

  await t.test('Should resolve Triangle variants explicitly', async () => {
    const op1 = compiler._resolveOperator('Triangle/equilateral');
    assert.strictEqual(op1.path, 'jot/Triangle/equilateral');
    
    const op2 = compiler._resolveOperator('Triangle/right');
    assert.strictEqual(op2.path, 'jot/Triangle/right');
  });

  await t.test('Should resolve Offset variants', async () => {
    const op1 = compiler._resolveOperator('offset');
    assert.strictEqual(op1.path, 'jot/offset');
    
    const op2 = compiler._resolveOperator('offset/closure');
    assert.strictEqual(op2.path, 'jot/offset/closure');
  });

  await t.test('Should throw on ambiguous Hexagon request', async () => {
    compiler.registerOperator('Hexagon/full', { path: 'jot/Hexagon/full', schema: {} });
    compiler.registerOperator('Hexagon/cap', { path: 'jot/Hexagon/cap', schema: {} });
    
    // Hexagon is ambiguous because both /full and /cap exist under the prefix
    await assert.rejects(() => compiler._evaluateCall({ name: 'Hexagon', args: [] }, {}), /ambiguous/);
  });
});
