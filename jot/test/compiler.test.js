import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';

test('JotCompiler Argument Mapping', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  await t.test('Singleton Harvesting (num, str, bool)', async () => {
    compiler.registerOperator('Test', {
      path: 'jot/Test',
      schema: {
        arguments: {
          n: { type: 'num' },
          s: { type: 'str' },
          b: { type: 'bool' }
        }
      }
    });
    const resolved = await compiler.evaluate(parser.parse('Test(10, "hello", true)'));
    assert.deepEqual(resolved.parameters, { n: 10, s: 'hello', b: true });
  });

  await t.test('Greedy Contiguous Harvesting (nums)', async () => {
    compiler.registerOperator('Poly', {
      path: 'jot/Poly',
      schema: {
        arguments: {
          points: { type: 'nums' },
          closed: { type: 'bool', default: true }
        }
      }
    });
    const resolved = await compiler.evaluate(parser.parse('Poly(1, 2, 3, 4, false)'));
    assert.deepEqual(resolved.parameters, { points: [1, 2, 3, 4], closed: false });
  });

  await t.test('Greedy Contiguous Harvesting (tags)', async () => {
    compiler.registerOperator('Style', {
      path: 'jot/Style',
      schema: {
        arguments: {
          id: { type: 'num' },
          traits: { type: 'tags' },
          active: { type: 'bool' }
        }
      }
    });
    const resolved = await compiler.evaluate(parser.parse('Style(42, "bold", "italic", true)'));
    assert.deepEqual(resolved.parameters, {
      id: 42,
      traits: { bold: true, italic: true },
      active: true
    });
  });

  await t.test('Positional Skip-over (Skipping VFS slots)', async () => {
    // offset(subject, $out, radius)
    compiler.registerOperator('offset', {
      path: 'jot/offset',
      schema: {
        arguments: {
          $in: { type: 'shape' },
          $out: { type: 'shape' },
          radius: { type: 'num' }
        }
      }
    });
    
    // Simulate Hexagon(30).offset(10)
    // The '10' should skip $in (subject) and $out (shape) to hit radius
    const subject = { path: 'jot/Hexagon', parameters: { diameter: 30 } };
    const ast = {
      type: 'METHOD',
      subject: subject,
      name: 'offset',
      args: [10]
    };
    
    const resolved = await compiler.evaluate(ast);
    assert.equal(resolved.path, 'jot/offset');
    assert.deepEqual(resolved.parameters.$in, subject);
    assert.equal(resolved.parameters.diameter, 20); // radius 10 normalized to diameter 20
  });

  await t.test('Structural Types (vec3, intv)', async () => {
    compiler.registerOperator('Move', {
      path: 'jot/Move',
      schema: {
        arguments: {
          pos: { type: 'vec3' },
          range: { type: 'intv' }
        }
      }
    });

    const res1 = await compiler.evaluate(parser.parse('Move([1, 2, 3], 10)'));
    assert.deepEqual(res1.parameters.pos, [1, 2, 3]);
    assert.deepEqual(res1.parameters.range, [-5, 5]); // intv normalization

    const res2 = await compiler.evaluate(parser.parse('Move([0, 0, 0], [10, 20])'));
    assert.deepEqual(res2.parameters.range, [10, 20]); // explicit intv
  });

  await t.test('Greedy Jots Harvesting', async () => {
    compiler.registerOperator('Assemble', {
      path: 'jot/Assemble',
      schema: {
        arguments: {
          children: { type: 'jots' },
          name: { type: 'str' }
        }
      }
    });

    // Mock sub-operators
    compiler.registerOperator('A', { path: 'jot/A' });
    compiler.registerOperator('B', { path: 'jot/B' });

    const resolved = await compiler.evaluate(parser.parse('Assemble(A(), B(), "my-assembly")'));
    assert.equal(resolved.parameters.children.length, 2);
    assert.equal(resolved.parameters.children[0].path, 'jot/A');
    assert.equal(resolved.parameters.children[1].path, 'jot/B');
    assert.equal(resolved.parameters.name, 'my-assembly');
  });

  await t.test('Contiguity Constraint', async () => {
    compiler.registerOperator('Mixed', {
      path: 'jot/Mixed',
      schema: {
        arguments: {
          numbers: { type: 'nums' },
          other: { type: 'num' }
        }
      }
    });

    // In a pure greedy model without contiguity, '1, 2, 3' might all be swallowed.
    // But if we have another 'num' slot, it should be possible to stop.
    // However, the greedy consumer by definition swallows ALL matching.
    // So 'Mixed(1, 2, 3)' -> numbers: [1, 2, 3], other: undefined
    const res = await compiler.evaluate(parser.parse('Mixed(1, 2, 3)'));
    assert.deepEqual(res.parameters.numbers, [1, 2, 3]);
    assert.equal(res.parameters.other, undefined);
  });
});
