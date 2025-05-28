import {
  Op,
  beginOps,
  endOps,
  ops,
  predicateValueHandler,
  resolve,
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

export const Plus = Op.registerOp(
  'Plus',
  ['number', ['number'], 'number'],
  (context, input = 0, value) => input + value
);

export const Say = Op.registerOp(
  'Say',
  ['number', ['string', 'string'], 'number'],
  (context, input, set, value) => {
    emit(set, value);
    return input;
  }
);

describe('op', () => {
  it('should resolve a single op', async () => {
    beginOps();
    Plus(1);
    const context = {};
    const graph = {};
    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    });
  });

  it('should resolve an op', async (t) => {
    beginOps();
    Plus(1).Plus(2);
    const context = {};
    const graph = {};
    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' H6F6k3Z7kUDXFLCv7pochRLLcABMpMtsQjfOmdC1dVg=': 3,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    });
  });

  it('should resolve an op with an argument', async () => {
    beginOps();
    Plus(1).Plus(Plus(5));
    const context = {};
    const graph = {};
    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' ghBkIkq4TM++8L1DCkPzO20T1PiI/qpEpZu8hM2q4FE=': 7,
      ' mEkVZKR9NY3KetPPXL036R8cHPZbeno6nY/VhcpykZo=': 6,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    });
  });

  it('should only recompute changed subgraphs', async () => {
    beginOps();
    Plus(1).Say('recompute', 'hello').Say('recompute', 'world');
    const context = {};
    const graph = {};
    await resolve(context, graph, ops);
    assert.deepEqual(graph, {
      ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
      ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
    });
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    // The graph remains resolved.
    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
      ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
    });

    // Since Say() was not recomputed we see 'hello', 'world' once.
    assert.deepEqual(emits['recompute'], ['hello', 'world']);

    beginOps();
    Plus(1).Say('recompute', 'hello').Say('recompute', 'world!');

    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
      ' miiMCCN8fu0OBEH4bD+sOIi8adyEIvxX8EDoq+mm+e0=': 1,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
      ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
    });

    // Since only the second Say() was recomputed we see 'hello', 'world', 'world!'.
    assert.deepEqual(emits['recompute'], ['hello', 'world', 'world!']);

    beginOps();
    Plus(1).Say('recompute', 'hello!').Say('recompute', 'world!');

    await resolve(context, graph, ops);
    endOps();
    assert.deepEqual(graph, {
      ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
      ' G5UU8j16G4Ey5p/QPlBvyB6a2e8Tg/IYnG/srrPCquI=': 1,
      ' KkJyemrsf0Ggyg7roFm7N0AyDX85yCK7iyLivFk5kYE=': 1,
      ' miiMCCN8fu0OBEH4bD+sOIi8adyEIvxX8EDoq+mm+e0=': 1,
      ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
      ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
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
