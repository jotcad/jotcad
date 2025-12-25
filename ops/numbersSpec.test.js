import { Op, run } from '@jotcad/op';
import { describe, it } from 'node:test';

import { strict as assert } from 'node:assert';
import { numbersSpecHandler } from './numbersSpec.js'; // Import the handler

describe('numbersSpec', () => {
  it('should generate a sequence of numbers with "by" directly from handler', () => {
    const spec = 'numbers';
    const caller = {}; // Mock caller
    const args = [{ by: 1 / 8 }]; // The argument we want to test
    const rest = []; // Empty rest array

    const result = numbersSpecHandler(spec, caller, args, rest);

    assert.deepStrictEqual(
      result,
      [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
    );
  });

  // I will keep the previous test as well, but fix its spec.
  it('should register numbers spec handler and work with op', async () => {
    // Ensure the handler is registered
    assert.ok(Op.hasSpecHandler('numbers'));

    const testOp = Op.registerOp({
      name: 'testOp',
      spec: [null, ['numbers'], 'numbers'], // Corrected output spec
      code: (id, session, input, numbers) => numbers,
    });

    const graph = await run({}, () => testOp({ by: 1 / 8 }));
    const result = graph[Object.keys(graph).pop()];
    assert.deepStrictEqual(
      await result,
      [0, 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875]
    );
  });
});
