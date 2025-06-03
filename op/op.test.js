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
  code: (context, input = 0, value) => input + value,
});

export const Say = Op.registerOp({
  name: 'Say',
  spec: ['number', ['string', 'string'], 'number'],
  code: (context, input, set, value) => {
    emit(set, value);
    return input;
  },
});

describe('op', () => {
  it('should resolve a single op', async () => {
    const graph = await run({}, () => Plus(1));
    assert.deepEqual(graph, {
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });
  });

  it('should resolve an op', async (t) => {
    const graph = await run({}, () => Plus(1).Plus(2));
    assert.deepEqual(graph, {
      ' X8NguZFn2KML+oEjdUEtxijaZ78cO90OnmId4q/VDEM=': 3,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });
  });

  it('should resolve an op with an argument', async () => {
    const graph = await run({}, () => Plus(1).Plus(Plus(5)));
    assert.deepEqual(graph, {
      ' 1Gshky5DzE54oJCYlDuAGQXQsNVp9bnU8CZEmftasdI=': 6,
      ' GBoCpRj6SVnSNFkNcG5auMP8isEJCFWMokJc2TF06jg=': 7,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });
  });

  it('should only recompute changed subgraphs', async () => {
    const graph1 = await run({}, () =>
      Plus(1).Say('recompute', 'hello').Say('recompute', 'world')
    );
    assert.deepEqual(graph1, {
      ' SYq7C0qyoqwTKUDMyZnR14JRU73NpketIccwh7tEBV8=': 1,
      ' oJaKB8L2k/oeMZbQeddD90yevN5BRqVNQzBJDVxaryU=': 1,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    const graph2 = await run({}, () => undefined, graph1);
    assert.deepEqual(graph2, {
      ' SYq7C0qyoqwTKUDMyZnR14JRU73NpketIccwh7tEBV8=': 1,
      ' oJaKB8L2k/oeMZbQeddD90yevN5BRqVNQzBJDVxaryU=': 1,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });

    // Since Say() was not recomputed we see 'hello', 'world' once.
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    const graph3 = await run(
      {},
      () => Plus(1).Say('recompute', 'hello').Say('recompute', 'world!'),
      graph2
    );

    assert.deepEqual(graph3, {
      ' SYq7C0qyoqwTKUDMyZnR14JRU73NpketIccwh7tEBV8=': 1,
      ' e7Iet5QOUW613v2zDH0O1i3ZBesS+ZuG/jCAcsiGiDs=': 1,
      ' oJaKB8L2k/oeMZbQeddD90yevN5BRqVNQzBJDVxaryU=': 1,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
    });

    // Since only the second Say() was recomputed we see 'hello', 'world', 'world!'.
    assert.deepEqual(emits['recompute'], ['hello', 'world', 'world!']);

    const graph4 = await run(
      {},
      () => Plus(1).Say('recompute', 'hello!').Say('recompute', 'world!'),
      graph3
    );

    assert.deepEqual(graph4, {
      ' 5ncnXdM9yNN/8eYEbamRkDiwK2e+9/AI0b89i6WbHR0=': 1,
      ' SYq7C0qyoqwTKUDMyZnR14JRU73NpketIccwh7tEBV8=': 1,
      ' e7Iet5QOUW613v2zDH0O1i3ZBesS+ZuG/jCAcsiGiDs=': 1,
      ' oJaKB8L2k/oeMZbQeddD90yevN5BRqVNQzBJDVxaryU=': 1,
      ' t1IUthHGe2PJ6Hs1bNxtZeWv7lShL6+WIzb1WAsVCi4=': 1,
      ' wgAulLiQM7eUCDWtA4K+nj6pvp0ZEmKEwEe80M1mjSI=': 1,
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
});
