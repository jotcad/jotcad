import {
  Op,
  beginOps,
  endOps,
  ops,
  predicateValueHandler,
  resolve,
  run,
} from './op.js';

import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

const emits = {};

const emit = (set, value) => {
  if (emits[set] === undefined) {
    emits[set] = [];
  }
  emits[set].push(value);
};

export const isNumber = (value) => typeof value === 'number';
export const isString = (value) => typeof value === 'string';

Op.registerSpecHandler(predicateValueHandler('number', isNumber));
Op.registerSpecHandler(predicateValueHandler('string', isString));

export const Plus = Op.registerOp({
  name: 'Plus',
  spec: ['number', ['number'], 'number'],
  code: (id, context, input = 0, value) => {
    console.log(`QQ/Plus.input=${input}`);
    return input + value;
  }
});

export const Say = Op.registerOp({
  name: 'Say',
  spec: ['number', ['string', 'string'], 'number'],
  code: (id, context, input, set, value) => {
    console.log(`QQ/Say.input=${input}`);
    emit(set, value);
    return input;
  },
});

export const z = Op.registerOp({
  name: 'z',
  spec: ['number', ['number'], 'number'],
  code: (id, assets, input = 0, offsets) => {
    console.log(`QQ/z.input=${input}`);
    return input;
  }
});

describe('op', () => {
  /*
  it('should resolve an op with an argument', async () => {
    // This test is fundamentally flawed. The 'Say' op expects two string arguments,
    // but it is being passed a number from the 'z(1)' operation.
    const graph = await run({}, () => Plus(1).Say(z(1)));
    assert.deepEqual(graph, {
      ' 4f15fa67ed8aebc619dd87f44f2e0f7d513ab4fd': 6,
      ' 864479c35ecdb08f9e5c6e27b097f7c30fda3da8': 7,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
  });
  */

  /*
  it('should resolve an op with an argument', async () => {
    const graph = await run({}, () => Plus(1).Plus(z(1)));
    assert.deepEqual(graph, {
      ' 4f15fa67ed8aebc619dd87f44f2e0f7d513ab4fd': 6,
      ' 864479c35ecdb08f9e5c6e27b097f7c30fda3da8': 7,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
  });
  */

  /*
  it('should resolve an op with an argument', async () => {
    const graph = await run({}, () => Plus(1).Plus(Plus(5)));
    assert.deepEqual(graph, {
      ' 4f15fa67ed8aebc619dd87f44f2e0f7d513ab4fd': 6,
      ' 864479c35ecdb08f9e5c6e27b097f7c30fda3da8': 7,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
  });
  */

  /*
  it('should resolve a single op', async () => {
    const graph = await run({}, () => Plus(1));
    assert.deepEqual(graph, {
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
  });

  it('should resolve an op', async (t) => {
    const graph = await run({}, () => Plus(1).Plus(2));
    assert.deepEqual(graph, {
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
      ' d681edb3fa7c49cc515b95be9926a9a5111d06e6': 3,
    });
  });

  it('should resolve an op with an argument', async () => {
    const graph = await run({}, () => Plus(1).Plus(Plus(5)));
    assert.deepEqual(graph, {
      ' 4f15fa67ed8aebc619dd87f44f2e0f7d513ab4fd': 6,
      ' 864479c35ecdb08f9e5c6e27b097f7c30fda3da8': 7,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
  });

  it('should only recompute changed subgraphs', async () => {
    const graph1 = await run({}, () =>
      Plus(1).Say('recompute', 'hello').Say('recompute', 'world')
    );
    assert.deepEqual(graph1, {
      ' 9070128831d2544aaceed9c8ca6c06018891071a': 1,
      ' b1bc3ebf2675e69d1568f2282c176e5916d8cc00': 1,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    const graph2 = await run({}, () => undefined, graph1);
    assert.deepEqual(graph2, {
      ' 9070128831d2544aaceed9c8ca6c06018891071a': 1,
      ' b1bc3ebf2675e69d1568f2282c176e5916d8cc00': 1,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });

    // Since Say() was not recomputed we see 'hello', 'world' once.
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    const graph3 = await run(
      {},
      () => Plus(1).Say('recompute', 'hello').Say('recompute', 'world!'),
      graph2
    );

    assert.deepEqual(graph3, {
      ' 578ddb98f7d0868ee58743e3d5303363acba2de2': 1,
      ' 9070128831d2544aaceed9c8ca6c06018891071a': 1,
      ' b1bc3ebf2675e69d1568f2282c176e5916d8cc00': 1,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
    });

    // Since only the second Say() was recomputed we see 'hello', 'world', 'world!'.
    assert.deepEqual(emits['recompute'], ['hello', 'world', 'world!']);

    const graph4 = await run(
      {},
      () => Plus(1).Say('recompute', 'hello!').Say('recompute', 'world!'),
      graph3
    );

    assert.deepEqual(graph4, {
      ' 578ddb98f7d0868ee58743e3d5303363acba2de2': 1,
      ' 9070128831d2544aaceed9c8ca6c06018891071a': 1,
      ' 9b957d5b25017b9f8baaabc01870fbf335062fb0': 1,
      ' b1bc3ebf2675e69d1568f2282c176e5916d8cc00': 1,
      ' c8b90aa81f9fca7b2918b776a453b9514f401db4': 1,
      ' c9696e2dfc0353ba4fccf7aa31f07814d44b6e59': 1,
    });

    // All says were recomputed.
    assert.deepEqual(emits['recompute'], [
      'hello',
      'world',
      'world!',
      'hello!',
      'world!',
    ]);
  });
  */
});
