import {
  Op,
  beginOps,
  endOps,
  ops,
  predicateValueHandler,
  resolve,
} from './op.js';
import test from 'ava';

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

test('Test single op', async (t) => {
  beginOps();
  Plus(1);
  const context = {};
  const graph = {};
  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, { ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1 });
});

test('Test op chain', async (t) => {
  beginOps();
  Plus(1).Plus(2);
  const context = {};
  const graph = {};
  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, {
    ' H6F6k3Z7kUDXFLCv7pochRLLcABMpMtsQjfOmdC1dVg=': 3,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
  });
});

test('Test op argument', async (t) => {
  beginOps();
  Plus(1).Plus(Plus(5));
  const context = {};
  const graph = {};
  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, {
    ' ghBkIkq4TM++8L1DCkPzO20T1PiI/qpEpZu8hM2q4FE=': 7,
    ' mEkVZKR9NY3KetPPXL036R8cHPZbeno6nY/VhcpykZo=': 6,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
  });
});

test('Test recompute', async (t) => {
  beginOps();
  Plus(1).Say('recompute', 'hello').Say('recompute', 'world');
  const context = {};
  const graph = {};
  await resolve(context, graph, ops);
  t.deepEqual(graph, {
    ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
  });
  t.deepEqual(emits['recompute'], ['hello', 'world']);

  // The graph remains resolved.
  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, {
    ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
  });

  // Since Say() was not recomputed we see 'hello', 'world' once.
  t.deepEqual(emits['recompute'], ['hello', 'world']);

  beginOps();
  Plus(1).Say('recompute', 'hello').Say('recompute', 'world!');

  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, {
    ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
    ' miiMCCN8fu0OBEH4bD+sOIi8adyEIvxX8EDoq+mm+e0=': 1,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
  });

  // Since only the second Say() was recomputed we see 'hello', 'world', 'world!'.
  t.deepEqual(emits['recompute'], ['hello', 'world', 'world!']);

  beginOps();
  Plus(1).Say('recompute', 'hello!').Say('recompute', 'world!');

  await resolve(context, graph, ops);
  endOps();
  t.deepEqual(graph, {
    ' 8QqKQon47Dg0I49ETgU7Av+jukFaGX7uqDrL4hYoV+g=': 1,
    ' G5UU8j16G4Ey5p/QPlBvyB6a2e8Tg/IYnG/srrPCquI=': 1,
    ' KkJyemrsf0Ggyg7roFm7N0AyDX85yCK7iyLivFk5kYE=': 1,
    ' miiMCCN8fu0OBEH4bD+sOIi8adyEIvxX8EDoq+mm+e0=': 1,
    ' qTpGXcksSiPD5sdeFbjL2Rji/0tjowby2sn9P6ZRbxk=': 1,
    ' rpAgjXymLt6UWwl75415sMJVJPZ3ZlP/lpS42Hdr62E=': 1,
  });

  // All says were recomputed.
  t.deepEqual(emits['recompute'], [
    'hello',
    'world',
    'world!',
    'hello!',
    'world!',
  ]);
});
