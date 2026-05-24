import test from 'node:test';
import assert from 'node:assert';
import { JotParser } from '../src/parser.js';
import { JotCompiler } from '../src/compiler.js';
import { log } from '../../fs/src/log.js';
import { Selector } from '../../fs/src/vfs_core.js';

test('Integration: Box(15).Red().rz(0.25) Execution', async (t) => {
  const compiler = new JotCompiler();
  const parser = new JotParser();

  // 1. Setup Built-in Schemas
  const boxSchema = { arguments: [{ name: 'size', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };
  const colorSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'c', type: 'jot:string' }], outputs: { $out: 'jot:shape' } };
  const rzSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [{ name: 'a', type: 'jot:number' }], outputs: { $out: 'jot:shape' } };

  compiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
  compiler.registerOperator('Color', { path: 'jot/Color', schema: colorSchema });
  compiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

  // 2. Setup User Operator 'Red'
  const redScript = '$in.Color("red") -> $out';
  const redSchema = { inputs: { '$in': { type: 'jot:shape' } }, arguments: [], outputs: { $out: 'jot:shape' } };
  compiler.registerOperator('user/Red', { path: 'user/Red', schema: redSchema });

  // 3. Mock VFS that resolves User Ops recursively
  const mockVfs = {
    async read(selector) {
      // If it's a User Op, expand it first
      if (selector.path.startsWith('user/')) {
          const innerCompiler = new JotCompiler(this);
          innerCompiler.registerOperator('Box', { path: 'jot/Box', schema: boxSchema });
          innerCompiler.registerOperator('Color', { path: 'jot/Color', schema: colorSchema });
          innerCompiler.registerOperator('rz', { path: 'jot/rz', schema: rzSchema });

          const ast = (new JotParser()).parse(redScript);
          const results = await innerCompiler.evaluate(ast, selector.parameters, redSchema);
          
          // The result of a User Op evaluation is another selector. Read THAT.
          return await this.read(results[0].selector);
      }

      // If it's a Built-in, recursively read its parameters to "fulfill" it
      if (selector.path.startsWith('jot/')) {
          const fulfilledParams = {};
          for (const [key, val] of Object.entries(selector.parameters)) {
              if (val instanceof Selector || val?._isJotSelector) {
                  fulfilledParams[key] = await this.read(Selector.fromObject(val));
              } else {
                  fulfilledParams[key] = val;
              }
          }
          const fulfilled = Selector.fromObject(selector.toJSON());
          fulfilled.parameters = fulfilledParams;
          return fulfilled;
      }

      return selector;
    }
  };

  // 4. Compile main script
  const mainScript = 'Box(15).Red().rz(0.25) -> $out';
  const mainTerminals = await compiler.evaluate(parser.parse(mainScript), {}, { outputs: { $out: 'jot:shape' } });
  const finalSelector = mainTerminals[0].selector;

  // 5. Execution (Full Read Fulfillment)
  log('--- STARTING FULL READ ---');
  const fulfillment = await mockVfs.read(finalSelector);

  // Assertions
  assert.strictEqual(fulfillment.path, 'jot/rz', 'Final result should be rz');
  assert.strictEqual(fulfillment.parameters.$in.path, 'jot/Color', 'rz subject should be Color');
  assert.strictEqual(fulfillment.parameters.$in.parameters.$in.path, 'jot/Box', 'Color subject should be Box');
  assert.strictEqual(fulfillment.parameters.$in.parameters.$in.parameters.size, 15);
  
  log('--- FULL FULFILLMENT REACHED ---');
  log(JSON.stringify(fulfillment, null, 2));
});
