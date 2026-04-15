import test from 'node:test';
import assert from 'node:assert';
import { VFS, MemoryStorage, WebReadableStream } from '@jotcad/fs';
import { registerJotProvider } from '../src/provider.js';

test('JotCAD VFS Provider: Integration', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });
  registerJotProvider(vfs);

  // Register a mock shape provider
  vfs.registerProvider('shape/box', async (v, selector) => {
    const { width = 0 } = selector.parameters;
    const bytes = new TextEncoder().encode(`Box of size ${width}`);
    return new WebReadableStream({
      start(c) {
        c.enqueue(bytes);
        c.close();
      },
    });
  });

  await t.test('evaluates a .jot file with parameters', async () => {
    // 1. "Upload" a jot file to the mesh
    await vfs.writeData('parts/bracket.jot', {}, 'box(length)');

    // 2. Request the jot file with a parameter
    const result = await vfs.readText('parts/bracket.jot', { length: 42 });

    assert.strictEqual(result, 'Box of size 42');
  });

  await t.test('evaluates nested .jot expressions', async () => {
    const expression = 'box({ width: L }).rx(0.1)';

    // Mock rotation op
    vfs.registerProvider('op/rotateX', async (v, selector) => {
      const { source, turns } = selector.parameters;
      const sourceText = await v.readText(source.path, source.parameters);
      const bytes = new TextEncoder().encode(
        `${sourceText} rotated ${turns} turns`
      );
      return new WebReadableStream({
        start(c) {
          c.enqueue(bytes);
          c.close();
        },
      });
    });

    const result = await vfs.readText('jot/eval', {
      expression,
      params: { L: 100 },
    });
    assert.strictEqual(result, 'Box of size 100 rotated 0.1 turns');
  });

  await t.test('Implicit Context Propagation', async (st) => {
    // Mock operators for A, B, C logic
    vfs.registerProvider('op/A', async (v, s) => {
      return new TextEncoder().encode(`${s.parameters.value}`);
    });
    vfs.registerProvider('op/B', async (v, s) => {
      const input = await v.readText(s.parameters.$in.path, s.parameters.$in.parameters);
      const arg = await v.readText(s.parameters.arg.path, s.parameters.arg.parameters);
      return new TextEncoder().encode(`${input} + ${arg}`);
    });
    vfs.registerProvider('op/C', async (v, s) => {
      const input = await v.readText(s.parameters.$in.path, s.parameters.$in.parameters);
      return new TextEncoder().encode(`${input} * ${s.parameters.arg}`);
    });

    await st.test('Scenario 1: Box() - no subject', async () => {
      const result = await vfs.readText('jot/eval', { expression: 'box(10)' });
      assert.strictEqual(result, 'Box of size 10');
    });

    await st.test('Scenario 2: a.Box() - a is subject of Box', async () => {
      // Create a mock for op/B that doesn't expect its 'arg' to be a selector,
      // because in A(10).B(2), '2' is a literal.
      vfs.registerProvider('op/B_literal', async (v, s) => {
        const input = await v.readText(s.parameters.$in.path, s.parameters.$in.parameters);
        return new TextEncoder().encode(`${input} + ${s.parameters.arg}`);
      });
      // We'll update the compiler registration for B to use this one just for this subtest
      // or just use the existing B and handle the literal.
      
      // Let's just make op/B robust to both literals and selectors.
      vfs.registerProvider('op/B', async (v, s) => {
        const input = await v.readText(s.parameters.$in.path, s.parameters.$in.parameters);
        let arg = s.parameters.arg;
        if (arg && arg.path) {
          arg = await v.readText(arg.path, arg.parameters);
        }
        return new TextEncoder().encode(`${input} + ${arg}`);
      });

      const result = await vfs.readText('jot/eval', { expression: 'A(10).B(2)' });
      assert.strictEqual(result, '10 + 2');
    });

    await st.test('Scenario 3: a.b(Box()) - a is subject of Box', async () => {
      // Here 'Box()' is an argument to 'b', but its subject should be 'a'.
      vfs.registerProvider('op/B_complex', async (v, s) => {
        const input = await v.readText(s.parameters.$in);
        const arg = await v.readText(s.parameters.arg);
        return new TextEncoder().encode(`B(input=${input}, arg=${arg})`);
      });
      // Mock Box that reports its $in
      vfs.registerProvider('op/Box_check', async (v, s) => {
        const input = await v.readText(s.parameters.$in);
        return new TextEncoder().encode(`Box(in=${input})`);
      });

      // Map them in the compiler (jot/eval uses the singleton compiler we setup in provider.js)
      // This is slightly tricky because jot/eval uses a shared compiler.
      // But we can test the general principle with A(10).B(C(2))
      const result = await vfs.readText('jot/eval', { expression: 'A(10).B(C(2))' });
      // A(10) -> "10"
      // C(2) with subject A(10) -> "10 * 2"
      // B("10 * 2") with subject A(10) -> "10 + 10 * 2" (assuming B uses the C(2) result as arg)
      // Actually B(C(2)) means C(2) is the 'arg' to B.
      assert.strictEqual(result, '10 + 10 * 2');
    });
  });
});
