import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCompiler Argument Mapping & Annotations', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  await t.test('Positional Mapping (Basic)', async () => {
    compiler.registerOperator('jot/Box', {
      path: 'jot/Box',
      schema: {
        arguments: [
          { name: 'radius', type: 'jot:number' },
          { name: 'depth', type: 'jot:number', default: 1 }
        ],
        outputs: { "$out": { type: "jot:shape" } }
      }
    });
    const res = await compiler.evaluate(parser.parse('Box(10)'));
    assert.strictEqual(res.parameters.radius, 10);
  });

  await t.test('Named Argument Mapping (Name=Value)', async () => {
    const res = await compiler.evaluate(parser.parse('Box(depth=5, radius=20)'));
    assert.strictEqual(res.parameters.radius, 20);
    assert.strictEqual(res.parameters.depth, 5);
  });

  // FAIL: Fails with "Missing required argument 'tools'" due to the affiliate blocking bug
  // in _satisfySchema when evaluating template arguments.
  await t.test('Type Annotation Mapping (Type:Value)', async () => {
    // Box(10).at(jot:number:5, op:cut(Box(1)))
    // This tests that type hints guide the mapping
    compiler.registerOperator('jot/at', {
        path: 'jot/at',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'target', type: 'jot:shape' },
                { name: 'op', type: 'jot:op<$in:shape, $out:shape>' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });
    compiler.registerOperator('jot/cut', {
        path: 'jot/cut',
        schema: {
            arguments: [
                { name: '$in', type: 'jot:shape', affiliate: '$out' },
                { name: 'tools', type: 'jot:shapes' }
            ],
            outputs: { "$out": { type: "jot:shape" } }
        }
    });

    const ast = parser.parse('Box(10).at(Box(2), cut(Box(1)))');
    const res = await compiler.evaluate(ast);
    assert.strictEqual(res.parameters.target.parameters.radius, 2);
    assert.strictEqual(res.parameters.op.path, 'jot/cut');
  });

  await t.test('Lookahead Failure (Strict Mismatch)', async () => {
    // If I pass a shape to a number slot, it should throw a "Missing required argument" error
    // because the number consumer won't take the shape.
    await assert.rejects(async () => {
        await compiler.evaluate(parser.parse('Box(Box(10))'));
    }, /Missing required argument 'radius' for 'Box'/);
  });
});
