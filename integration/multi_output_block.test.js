import { runIntegrationTest } from './harness.js';
import assert from 'node:assert';
import { Selector } from '../fs/src/index.js';
import { vfsResult } from './vfs_test_helpers.js';

runIntegrationTest('Multi-Output Scoped Block Integration', async ({ t, vfs, compiler, parser, readData }) => {
  await t.test('Scoped block with multiple outputs correctly resolves ports', async () => {
    const script = 'B = Box(20); D = Disk(10); B.cut(D) -> $out; D.mx(5) -> extra';
    const ast = parser.parse(script);

    console.log('[Test] Evaluating block script...');
    const terminals = await compiler.evaluate(ast, {}, {
        outputs: {
            "$out": { type: "jot:shape" },
            "extra": { type: "jot:shape" }
        }
    });

    assert.strictEqual(terminals.length, 2, 'Should have 2 terminal outputs from block');
    
    const outResult = terminals.find(t => t.port === '$out');
    const extraResult = terminals.find(t => t.port === 'extra');

    assert.ok(outResult, 'Should find $out terminal');
    assert.ok(extraResult, 'Should find extra terminal');

    assert.strictEqual(outResult.selector.path, 'jot/cut', '$out should be the result of the cut op');
    assert.strictEqual(extraResult.selector.path, 'jot/mx', 'extra should be the result of the mx op');

    // Verify retrieval of both
    console.log('[Test] Requesting $out from VFS...');
    const shape1 = await readData(outResult.selector);
    assert.strictEqual(shape1.tags.type, 'surface');

    console.log('[Test] Requesting extra from VFS...');
    const shape2 = await readData(extraResult.selector);
    assert.strictEqual(shape2.tags.type, 'surface');
  });

  await t.test('Sub-selector port resolution across mesh', async () => {
    // Defining an op that returns multiple ports
    const opSchema = {
        path: 'user/MultiOut',
        arguments: [],
        outputs: {
            'main': { type: 'jot:shape' },
            'side': { type: 'jot:shape' }
        }
    };

    vfs.registerProvider('user/MultiOut', async (v, s) => {
        const script = 'Box(10) -> main; Disk(5) -> side';
        const results = await compiler.evaluate(parser.parse(script), {}, opSchema);
        const target = results.find(r => r.port === s.output);
        // Inject a tag to verify we got the right port
        const shape = await readData(target.selector);
        shape.tags.port = s.output;
        
        // We still need the original metadata or similar. Let's read to get metadata
        const res = await v.read(target.selector);
        return vfsResult(shape, res.metadata);
    }, { schema: opSchema });

    console.log('[Test] Requesting user/MultiOut:side...');
    const sideSelector = new Selector('user/MultiOut').withOutput('side');
    const shape = await readData(sideSelector);
    
    assert.strictEqual(shape.tags.port, 'side', 'Result should be from the requested side port');
    assert.strictEqual(shape.tags.type, 'surface', 'Result should be a surface (Disk)');
  });
});
