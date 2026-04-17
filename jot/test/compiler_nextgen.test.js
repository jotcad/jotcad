import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('JotCAD Next-Gen: Universal Sequence Principle (Implicit Mapping)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  await t.test('maps operation over a sequence subject', () => {
    // [box(10), orb(20)].extrude(5)
    const ast = [
      { path: 'shape/box', parameters: { width: 10 } },
      { path: 'shape/orb', parameters: { diameter: 20 } },
    ];

    // Simulating the result of Parser generating a sequence with a trailing method
    // In practice, the parser will return a CALL/METHOD node that the compiler handles.
    // For now, let's verify the compiler logic for a sequence.
    const res = compiler.evaluate({
      path: 'op/extrude',
      parameters: {
        source: ast,
        height: 5,
      },
    });

    assert.ok(Array.isArray(res), 'Should return a sequence');
    assert.strictEqual(res.length, 2);
    assert.strictEqual(res[0].path, 'op/extrude');
    assert.strictEqual(res[0].parameters.source.path, 'shape/box');
    assert.strictEqual(res[1].path, 'op/extrude');
    assert.strictEqual(res[1].parameters.source.path, 'shape/orb');
  });
});

test('JotCAD Next-Gen: Cartesian Product Generation', async (t) => {
  const compiler = new JotCompiler();

  await t.test('generates multiple shapes from sequence arguments', () => {
    // box(10).rx(0.1, 0.2)
    const opNode = {
      path: 'op/rotateX',
      parameters: {
        source: { path: 'shape/box', parameters: { width: 10 } },
        turns: [0.1, 0.2],
      },
    };

    const res = compiler.evaluate(opNode);

    assert.ok(Array.isArray(res), 'Should return a sequence');
    assert.strictEqual(res.length, 2);
    assert.strictEqual(res[0].parameters.turns, 0.1);
    assert.strictEqual(res[1].parameters.turns, 0.2);
  });

  await t.test('handles nested Cartesian products', () => {
    // box(10, 20).rx(0.1, 0.2) where box(10, 20) is [box(10), box(20)]
    const opNode = {
      path: 'op/rotateX',
      parameters: {
        source: [
          { path: 'shape/box', parameters: { width: 10 } },
          { path: 'shape/box', parameters: { width: 20 } },
        ],
        turns: [0.1, 0.2],
      },
    };

    const res = compiler.evaluate(opNode);

    assert.strictEqual(res.length, 4, 'Should generate 2x2 = 4 shapes');
    // box(10).rx(0.1)
    assert.strictEqual(res[0].parameters.source.parameters.width, 10);
    assert.strictEqual(res[0].parameters.turns, 0.1);
    // box(10).rx(0.2)
    assert.strictEqual(res[1].parameters.source.parameters.width, 10);
    assert.strictEqual(res[1].parameters.turns, 0.2);
    // box(20).rx(0.1)
    assert.strictEqual(res[2].parameters.source.parameters.width, 20);
    assert.strictEqual(res[2].parameters.turns, 0.1);
    // box(20).rx(0.2)
    assert.strictEqual(res[3].parameters.source.parameters.width, 20);
    assert.strictEqual(res[3].parameters.turns, 0.2);
  });
});

test('JotCAD Next-Gen: Universal Flattening', () => {
  const compiler = new JotCompiler();

  // [ [a, b], c ] -> [a, b, c]
  const ast = [
    [
      { path: 'shape/box', parameters: { width: 1 } },
      { path: 'shape/box', parameters: { width: 2 } },
    ],
    { path: 'shape/box', parameters: { width: 3 } },
  ];

  const res = compiler.evaluate(ast);
  assert.strictEqual(res.length, 3);
  assert.strictEqual(res[0].parameters.width, 1);
  assert.strictEqual(res[1].parameters.width, 2);
  assert.strictEqual(res[2].parameters.width, 3);
});

test('JotCAD Next-Gen: Dynamic Schema Loading (Preview)', async (t) => {
  const compiler = new JotCompiler();

  // Simulating what loadSchemas() would do
  compiler.registerOperator('box', {
    path: 'shape/box',
    schema: {
      properties: {
        width: { type: 'number', default: 10 },
        height: { type: 'number', default: 10 },
        depth: { type: 'number', default: 10 },
      },
    },
  });

  await t.test('resolves box(100) using schema property order', () => {
    // This requires Parser to return a CALL node
    const callNode = {
      type: 'CALL',
      name: 'box',
      args: [100],
    };

    const res = compiler.evaluate(callNode);
    assert.strictEqual(res.path, 'shape/box');
    assert.strictEqual(res.parameters.width, 100);
    assert.strictEqual(res.parameters.height, 10); // Default from schema
  });
});
