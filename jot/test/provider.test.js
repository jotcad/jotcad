import test from 'node:test';
import assert from 'node:assert/strict';
import { VFS, MemoryStorage, WebReadableStream, Selector } from '@jotcad/fs';
import { registerJotProvider } from '../src/provider.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCAD VFS Provider: Integration', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });
  const compiler = new JotCompiler(vfs);
  registerJotProvider(vfs, { compiler });

  // Register all mock schemas used in the tests
  const baseOutputs = { "$out": { type: "jot:shape" } };
  const anyOutputs = { "$out": { type: "jot:any" } };

  compiler.registerOperator('box', { 
    path: 'jot/box', 
    schema: { 
        arguments: [{ name: 'width', type: 'jot:number' }],
        outputs: baseOutputs
    } 
  });
  compiler.registerOperator('rx', { 
    path: 'jot/rotateX', 
    schema: { 
        arguments: [{ name: 'source', type: 'jot:shape', affiliate: '$out' }, { name: 'turns', type: 'jot:number' }],
        outputs: baseOutputs
    } 
  });
  compiler.registerOperator('A', { 
    path: 'jot/A', 
    schema: { 
        arguments: [{ name: 'value', type: 'jot:number' }],
        outputs: anyOutputs
    } 
  });
  compiler.registerOperator('B', { 
    path: 'jot/B', 
    schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'arg', type: 'jot:any' }],
        outputs: anyOutputs
    } 
  });
  compiler.registerOperator('C', { 
    path: 'jot/C', 
    schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'arg', type: 'jot:any' }],
        outputs: anyOutputs
    } 
  });

  // VFS-level provider registration
  vfs.registerProvider('jot/box', async (v, selector) => {
    const { width = 0 } = selector.parameters;
    const bytes = new TextEncoder().encode(`Box of size ${width}`);
    return new WebReadableStream({
      start(c) {
        c.enqueue(bytes);
        c.close();
      },
    });
  }, {
    schema: { 
        arguments: [{ name: 'width', type: 'jot:number' }],
        outputs: baseOutputs
    }
  });

  await t.test('evaluates a .jot file with parameters', async () => {
    // Write the .jot source file
    await vfs.writeData(new Selector('parts/bracket.jot'), 'box(length)');
    // Evaluate the .jot file by reading it with parameters
    const result = await vfs.readText(new Selector('parts/bracket.jot', { length: 42 }));
    assert.strictEqual(result, 'Box of size 42');
  });

  await t.test('evaluates nested .jot expressions', async () => {
    const expression = 'box(L).rx(0.1)';

    vfs.registerProvider('jot/rotateX', async (v, selector) => {
      const { source, turns } = selector.parameters;
      // 'source' is a Selector object here
      const sourceText = await v.readText(source);
      const bytes = new TextEncoder().encode(
        `${sourceText} rotated ${turns} turns`
      );
      return new WebReadableStream({
        start(c) {
          c.enqueue(bytes);
          c.close();
        },
      });
    }, {
      schema: { 
          arguments: [{ name: 'source', type: 'jot:shape', affiliate: '$out' }, { name: 'turns', type: 'jot:number' }],
          outputs: baseOutputs
      }
    });

    const result = await vfs.readText(new Selector('jot/eval', {
      expression,
      params: { L: 100 },
    }));
    assert.strictEqual(result, 'Box of size 100 rotated 0.1 turns');
  });

  await t.test('Implicit Context Propagation', async (st) => {
    vfs.registerProvider('jot/A', async (v, s) => {
      return new TextEncoder().encode(`${s.parameters.value}`);
    }, { schema: { 
        arguments: [{ name: 'value', type: 'jot:number' }],
        outputs: anyOutputs
    } });
    
    vfs.registerProvider('jot/B', async (v, s) => {
      const input = await v.readText(s.parameters.$in);
      let arg = s.parameters.arg;
      if (arg && arg.path) {
        arg = await v.readText(arg);
      }
      return new TextEncoder().encode(`${input} + ${arg}`);
    }, { schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'arg', type: 'jot:any' }],
        outputs: anyOutputs
    } });

    vfs.registerProvider('jot/C', async (v, s) => {
      const input = await v.readText(s.parameters.$in);
      return new TextEncoder().encode(`${input} * ${s.parameters.arg}`);
    }, { schema: { 
        arguments: [{ name: '$in', type: 'jot:shape', affiliate: '$out' }, { name: 'arg', type: 'jot:any' }],
        outputs: anyOutputs
    } });

    await st.test('Scenario 1: box() - no subject', async () => {
      const result = await vfs.readText(new Selector('jot/eval', { expression: 'box(10)' }));
      assert.strictEqual(result, 'Box of size 10');
    });

    await st.test('Scenario 2: a.B() - a is subject of B', async () => {
      const result = await vfs.readText(new Selector('jot/eval', { expression: 'A(10).B(2)' }));
      assert.strictEqual(result, '10 + 2');
    });

    // Scenario 3: a.B(C(2)) - a is subject of B, but NOT of C (due to Non-Propagation)
    // C(2) will fail because it's evaluated as a standard argument (not jot:op<>) 
    // and it's missing its required $in affiliate.
    await st.test('Scenario 3: a.B(C()) - a is NOT subject of C', async () => {
      await assert.rejects(
          vfs.readText(new Selector('jot/eval', { expression: 'A(10).B(C(2))' })),
          /Missing required argument '\$in' for 'C'/
      );
    });
  });
});
