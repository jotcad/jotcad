import { Op, predicateValueHandler, run } from './op.js';

import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

const mockShape = {
  move: (x, y, z) => mockShape,
  geometry: Symbol('mockGeometryId'),
};

export const isAny = (value) => true;

Op.registerSpecHandler(predicateValueHandler('any', isAny));

export const a = Op.registerOp({
  name: 'a',
  spec: [null, [], 'any'],
  code: (id, assets, input) => {
    return 'hello from a';
  },
});

export const b = Op.registerOp({
  name: 'b',
  spec: ['any', ['any'], 'any'], // 'any' for the argument and output
  code: (id, assets, input, arg) => {
    return arg + ' hello from b';
  },
});

export const c = Op.registerOp({
  name: 'c',
  spec: ['any', [], 'any'], // 'any' for the input and output
  code: (id, assets, input) => {
    return `${input} and hello from c`;
  },
});

describe('op', () => {
  it('should return "hello from a" for a()', async () => {
    const graph = await run({}, () => a());
    assert.deepEqual(
      graph[' 9a51f0f043f94f31cf0c02d91a93b89d0fd4b73f'],
      'hello from a'
    );
  });

  it('should have b() receive a() as input', async () => {
    const graph = await run({}, () => a().b(123));
    // We expect b's input to be 'hello from a', and its output to be 123 (the arg).
    // The ID for a() is ' 9a51f0f043f94f31cf0c02d91a93b89d0fd4b73f'
    // The ID for a().b(123) is ' e343a4cb2f0cbb9dc62ae641863cbfea6b80ef3a'
    const bOutput = graph[' e343a4cb2f0cbb9dc62ae641863cbfea6b80ef3a'];
    assert.deepEqual(bOutput, '123 hello from b');
  });

  it('should have b() and c() receive a() as input in a().b(c())', async () => {
    const graph = await run({}, () => a().b(c()));
    const aOutputSymbol = ' 9a51f0f043f94f31cf0c02d91a93b89d0fd4b73f';
    const cOutputSymbol = ' d59f1f6b44affaf9247cd992fd53b548f0748c9b';
    const bOutputSymbol = ' b5ed1ae52e99e78adcb3834d7c7dfd4a78bb0fa5';

    assert.deepEqual(
      graph[aOutputSymbol],
      'hello from a',
      'a() output should be "hello from a"'
    );
    assert.deepEqual(
      graph[cOutputSymbol],
      'hello from a and hello from c',
      'c() output should be "hello from a and hello from c"'
    );
    assert.deepEqual(
      graph[bOutputSymbol],
      'hello from a and hello from c hello from b',
      'b() output should be "hello from a and hello from c hello from b"'
    );
  });
});
