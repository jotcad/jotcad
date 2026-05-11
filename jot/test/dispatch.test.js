import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage } from '../../fs/src/vfs_core.js';
import { JotCompiler } from '../src/compiler.js';

test('Operator Variant Resolution and Dispatch', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });
  const compiler = new JotCompiler(vfs);

  // --- Triangle Variants ---
  compiler.registerOperator('Triangle/equilateral', { 
    path: 'jot/Triangle/equilateral', 
    schema: { 
        arguments: [{ name: 'base', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    } 
  });
  compiler.registerOperator('Triangle/right', { 
    path: 'jot/Triangle/right', 
    schema: { 
        arguments: [{ name: 'base', type: 'jot:number' }, { name: 'height', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    } 
  });

  // --- Offset Variants ---
  compiler.registerOperator('offset', { 
    path: 'jot/offset', 
    schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    } 
  });
  compiler.registerOperator('offset/closure', { 
    path: 'jot/offset/closure', 
    schema: { 
        arguments: [{ name: 'diameter', type: 'jot:number' }],
        outputs: { "$out": { type: "jot:shape" } }
    } 
  });

  await t.test('Should resolve Triangle variants explicitly', async () => {
    const ops = compiler._resolveOperator('Triangle/equilateral');
    assert.strictEqual(ops[0].path, 'jot/Triangle/equilateral');
  });

  await t.test('Should resolve Offset variants', async () => {
    const ops = compiler._resolveOperator('offset');
    assert.strictEqual(ops[0].path, 'jot/offset');
  });

  await t.test('Should resolve first matching variant when ambiguous', async () => {
    compiler.registerOperator('Hexagon', { 
        path: 'jot/Hexagon/full', 
        schema: { 
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        } 
    });
    compiler.registerOperator('Hexagon', { 
        path: 'jot/Hexagon/cap', 
        schema: { 
            arguments: [],
            outputs: { "$out": { type: "jot:shape" } }
        } 
    });
    
    // First match wins
    const schema = { outputs: { "$out": { type: "jot:shape" } } };
    const res = await compiler.evaluate({ 
        type: 'ASSIGNMENT', 
        name: '$out', 
        value: { type: 'CALL', name: 'Hexagon', args: [] } 
    }, {}, schema);
    const resolved = res[0];
    assert.strictEqual(resolved.path, 'jot/Hexagon/full');
  });
});
