
import test from 'node:test';
import assert from 'node:assert/strict';
import { VFS, MemoryStorage, Selector } from '@jotcad/fs';
import { JotCompiler } from '../src/compiler.js';
import { JotParser } from '../src/parser.js';

test('JotCompiler: Integration Mapping (Hexagon.cut)', async (t) => {
  const vfs = new VFS({ storage: new MemoryStorage() });
  const compiler = new JotCompiler(vfs);
  const parser = new JotParser();

  // 1. Register schemas representing the C++ operators
  compiler.registerOperator('Hexagon', {
    path: 'jot/Hexagon/full',
    schema: {
        arguments: [{ name: "diameter", type: "jot:number", default: 30 }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('Triangle', {
    path: 'jot/Triangle/equilateral',
    schema: {
        arguments: [{ name: "side", type: "jot:number", default: 10 }],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  compiler.registerOperator('cut', {
    path: 'jot/cut',
    schema: {
        inputs: { "$in": { type: "jot:shape" } },
        arguments: [
            { name: "tools", type: "jot:shapes", default: [] }
        ],
        outputs: { "$out": { type: "jot:shape" } }
    }
  });

  const defaultSchema = { outputs: { "$out": { type: "jot:shape" } } };

  await t.test('Hexagon(30).cut(Triangle(5)) - Correct Plural & Affiliate Mapping', async () => {
    const code = 'Hexagon(30).cut(Triangle(5)) -> $out';
    const ast = parser.parse(code);
    const res = await compiler.evaluate(ast, {}, defaultSchema);
    const sel = res[0].selector;

    // Verify 'tools' is automatically wrapped in an array (Plural awareness)
    assert.ok(Array.isArray(sel.parameters.tools), "'tools' must be an array");
    assert.strictEqual(sel.parameters.tools.length, 1);
    assert.strictEqual(sel.parameters.tools[0].path, 'jot/Triangle/equilateral');
    assert.strictEqual(sel.parameters.tools[0].parameters.side, 5);

    // Verify $in is correctly populated by the subject (Affiliate awareness)
    assert.ok(sel.parameters.$in, "'$in' must be populated");
    assert.strictEqual(sel.parameters.$in.path, 'jot/Hexagon/full');
    assert.strictEqual(sel.parameters.$in.parameters.diameter, 30);
  });

  await t.test('Hexagon(30).cut([Triangle(5), Triangle(10)]) - Multi-Plural Mapping', async () => {
    const code = 'Hexagon(30).cut([Triangle(5), Triangle(10)]) -> $out';
    const ast = parser.parse(code);
    const res = await compiler.evaluate(ast, {}, defaultSchema);
    const sel = res[0].selector;

    assert.ok(Array.isArray(sel.parameters.tools), "'tools' must be an array");
    assert.strictEqual(sel.parameters.tools.length, 2);
    assert.strictEqual(sel.parameters.tools[0].parameters.side, 5);
    assert.strictEqual(sel.parameters.tools[1].parameters.side, 10);
  });
});
