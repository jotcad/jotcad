import { Op, predicateValueHandler, run } from './op.js';
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';

const isNumber = (value) => typeof value === 'number';
Op.registerSpecHandler(predicateValueHandler('number', isNumber));

const A = Op.registerOp({
  name: 'A',
  spec: [null, ['number'], 'number'],
  code: (id, context, input, value) => value,
});

const B = Op.registerOp({
  name: 'B',
  spec: ['number', ['number'], 'number'],
  code: (id, context, input, arg) => input + arg,
});

const C = Op.registerOp({
  name: 'C',
  spec: ['number', ['number'], 'number'],
  code: (id, context, input, arg) => input * arg,
});

describe('implicit context', () => {
  it('a().b(c()) should be equivalent to a().b(a().c())', async () => {
    const graph = await run({}, () => A(10).B(C(2)));
    const lastOp = graph[Object.keys(graph).pop()];
    assert.equal(lastOp, 30);
  });

  it('should resolve input for argument op', async () => {
    const graph = await run({}, () => A(10).C(2));
    const lastOp = graph[Object.keys(graph).pop()];
    assert.equal(lastOp, 20); // 10 * 2
  });
});
