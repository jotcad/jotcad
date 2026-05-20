import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';

test('Compiler: Custom Operator Subject Injection ($in)', async (t) => {
  const parser = new JotParser();
  const compiler = new JotCompiler();

  // 1. Mock the geometry operators
  compiler.registerOperator('Box', {
    path: 'jot/Box',
    schema: {
      arguments: [
        { name: 'width', type: 'jot:number' },
        { name: 'height', type: 'jot:number' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('Disk', {
    path: 'jot/Disk',
    schema: {
      arguments: [{ name: 'radius', type: 'jot:number' }],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('corners', {
    path: 'jot/corners',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('nth', {
    path: 'jot/nth',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [
        { name: 'index', type: 'jot:number' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('o', {
    path: 'jot/origin',
    schema: {
      inputs: { '$in': { type: 'jot:shape', optional: true } },
      arguments: [],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('by', {
    path: 'jot/by',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [
        { name: 'target', type: 'jot:shape' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  compiler.registerOperator('clip', {
    path: 'jot/clip',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [
        { name: 'tool', type: 'jot:shape' }
      ],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  // 2. Register the custom 'Foot' operator
  // Script: $in.by(corners().nth(0).o()).clip(Disk(40)) -> $out
  compiler.registerOperator('user/Foot', {
    path: 'user/Foot',
    script: '$in.by(corners().nth(0).o()).clip(Disk(40)) -> $out',
    schema: {
      inputs: { '$in': { type: 'jot:shape' } },
      arguments: [],
      outputs: { $out: { type: 'jot:shape' } }
    }
  });

  // 3. Test 1: Box(100, 100).Foot()
  await t.test('Should inject subject into Foot method call and execute through VFS', async () => {
    // Mock VFS with a provider for user/Foot
    const mockVfs = {
      providers: new Map(),
      registerProvider(path, cb) { this.providers.set(path, cb); },
      async read(selector) {
        const provider = this.providers.get(selector.path);
        if (!provider) throw new Error(`No provider for ${selector.path}`);
        return await provider(this, selector);
      }
    };

    // Mimic JotRegistry.publishDynamicOp behavior
    mockVfs.registerProvider('user/Foot', async (v, s) => {
       const innerCompiler = new JotCompiler(v);
       // Register all the same ops for the inner compiler
       const ops = ['Box', 'Disk', 'corners', 'nth', 'o', 'by', 'clip'];
       for (const name of ops) {
         innerCompiler.registerOperator(name, compiler.operators.get(name)[0]);
       }

       const innerParser = new JotParser();
       const innerAst = innerParser.parse('$in.by(corners().nth(0).o()).clip(Disk(40)) -> $out');
       
       // Parameters come from the selector being "read"
       const params = { ...s.parameters };
       const result = await innerCompiler.evaluate(innerAst, params, { outputs: { $out: 'jot:shape' } });
       
       // Return the resulting selector as "data" for this test
       return result[0].selector;
    });

    const script = 'Box(100, 100).Foot() -> $out';
    const ast = parser.parse(script);
    
    // Outer evaluation produces the user/Foot selector
    const terminals = await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
    const footSel = terminals[0].selector;
    
    assert.strictEqual(footSel.path, 'user/Foot');
    assert.ok(footSel.parameters.$in, 'Subject $in should be present');

    // "Execute" the user op by reading it from the mock VFS
    const finalResult = await mockVfs.read(footSel);
    
    console.log('Final Resulting Selector:', JSON.stringify(finalResult, null, 2));
    
    // The final result should be the 'clip' selector from inside the script
    assert.strictEqual(finalResult.path, 'jot/clip');
    assert.strictEqual(finalResult.output, '$out');
    // Ensure the clip operation has the Box as its $in (via Foot's $in)
    assert.strictEqual(finalResult.parameters.$in.path, 'jot/by');
    assert.strictEqual(finalResult.parameters.tool.path, 'jot/Disk');
  });

  // 4. Test 2: Foot() - Should fail with missing $in
  await t.test('Should fail if Foot is called without a subject', async () => {
    const script = 'Foot() -> $out';
    const ast = parser.parse(script);
    
    try {
      await compiler.evaluate(ast, {}, { outputs: { $out: 'jot:shape' } });
      assert.fail('Should have thrown missing $in error');
    } catch (e) {
      assert.match(e.message, /Missing required input '\$in'/);
      console.log('Caught Expected Error:', e.message);
    }
  });
});
