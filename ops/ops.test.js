import { run } from '@jsxcad/op';
import { Triangle } from './main.js';
import { read } from '@jsxcad/sys';
import test from 'ava';

test('Simple', async t => {
  const graph = run(() => Triangle(10).color('blue').save('out'));
  t.deepEqual(await read('shape/out'), {});
});
