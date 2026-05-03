import { test } from 'node:test';
import assert from 'node:assert/strict';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('JotCompiler Argument Mapping', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  await t.test('Singleton Harvesting (num, str, bool)', async () => {
    compiler.registerOperator('Test', {
      path: 'jot/Test',
      schema: {
        arguments: [
          { name: 'n', type: 'jot:number' },
          { name: 's', type: 'jot:string' },
          { name: 'b', type: 'jot:boolean' }
        ],
        outputs: { "$out": { type: "jot:any" } }
      }
    });
    const resolved = await compiler.evaluate(parser.parse('Test(10, "hello", true)'));
    assert.deepEqual(resolved.parameters, { n: 10, s: 'hello', b: true });
    assert.strictEqual(resolved.output, '$out');
  });

  await t.test('Greedy Contiguous Harvesting (nums)', async () => {
    compiler.registerOperator('Poly', {
      path: 'jot/Poly',
      schema: {
        arguments: [
          { name: 'points', type: 'jot:numbers' },
          { name: 'closed', type: 'jot:boolean', default: true }
        ],
        outputs: { "$out": { type: "jot:shape" } }
      }
    });
    const resolved = await compiler.evaluate(parser.parse('Poly(1, 2, 3, 4, false)'));
    assert.deepEqual(resolved.parameters, { points: [1, 2, 3, 4], closed: false });
  });

  await t.test('Greedy Contiguous Harvesting (flags)', async () => {
    compiler.registerOperator('Style', {
      path: 'jot/Style',
      schema: {
        arguments: [
          { name: 'id', type: 'jot:number' },
          { name: 'traits', type: 'jot:flags' },
          { name: 'active', type: 'jot:boolean' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
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
        arguments: [
          { name: '$in', type: 'jot:shape', affiliate: '$out' },
          { name: '$out', type: 'jot:shape', default: null }, // default prevents blocking
          { name: 'radius', type: 'jot:number' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
      }
    });
    
    const subject = new Selector('jot/Hexagon', { diameter: 30 }).withOutput('$out');
    compiler.registerOperator('Hexagon', {
        path: 'jot/Hexagon',
        schema: {
            arguments: [{ name: 'diameter', type: 'jot:number' }],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    const ast = {
      type: 'METHOD',
      subject: subject,
      name: 'offset',
      args: [10]
    };
    
    const resolved = await compiler.evaluate(ast);
    assert.equal(resolved.path, 'jot/offset');
    assert.equal(resolved.parameters.$in.path, subject.path);
    assert.deepEqual(resolved.parameters.$in.parameters, subject.parameters);
    assert.equal(resolved.parameters.radius, 10);
    assert.strictEqual(resolved.output, '$out');
  });

  await t.test('Structural Types (vec3, interval)', async () => {
    compiler.registerOperator('Move', {
      path: 'jot/Move',
      schema: {
        arguments: [
          { name: 'pos', type: 'jot:vec3' },
          { name: 'range', type: 'jot:interval' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
      }
    });

    const res1 = await compiler.evaluate(parser.parse('Move([1, 2, 3], 10)'));
    assert.deepEqual(res1.parameters.pos, [1, 2, 3]);
    assert.deepEqual(res1.parameters.range, [-5, 5]); // interval normalization

    const res2 = await compiler.evaluate(parser.parse('Move([0, 0, 0], [10, 20])'));
    assert.deepEqual(res2.parameters.range, [10, 20]); // explicit interval

    // Implicit 0-start interval: [5] -> [0, 5]
    const res3 = await compiler.evaluate(parser.parse('Move([0, 0, 0], [5])'));
    assert.deepEqual(res3.parameters.range, [0, 5]);
  });

  await t.test('Greedy Jots Harvesting', async () => {
    compiler.registerOperator('Assemble', {
      path: 'jot/Assemble',
      schema: {
        arguments: [
          { name: 'children', type: 'jot:shapes' },
          { name: 'name', type: 'jot:string' }
        ],
        outputs: { "$out": { type: "jot:shape" } }
      }
    });

    // Mock sub-operators
    compiler.registerOperator('A', { path: 'jot/A', schema: { arguments: [], outputs: {"$out":{ type: 'jot:shape' }} } });
    compiler.registerOperator('B', { path: 'jot/B', schema: { arguments: [], outputs: {"$out":{ type: 'jot:shape' }} } });

    const resolved = await compiler.evaluate(parser.parse('Assemble(A(), B(), "my-assembly")'));
    assert.equal(resolved.parameters.children.length, 2);
    assert.equal(resolved.parameters.children[0].path, 'jot/A');
    assert.equal(resolved.parameters.children[1].path, 'jot/B');
    assert.equal(resolved.parameters.name, 'my-assembly');
  });

  // This test MUST fail because 'jot:numbers' is greedy and swallows all 3 numbers,
  // leaving the mandatory 'other' slot unsatisfied.
  await t.test('Contiguity Constraint', async () => {
    compiler.registerOperator('Mixed', {
      path: 'jot/Mixed',
      schema: {
        arguments: [
          { name: 'numbers', type: 'jot:numbers' },
          { name: 'other', type: 'jot:number' }
        ],
        outputs: { "$out": { type: "jot:any" } }
      }
    });

    await assert.rejects(
      async () => await compiler.evaluate(parser.parse('Mixed(1, 2, 3)')),
      /Missing required argument 'other'/
    );
  });
});
