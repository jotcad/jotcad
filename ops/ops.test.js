import { Box2 } from './box.js';
import { color } from './color.js';
import { read } from '@jotcad/sys';
import { run } from '@jotcad/op';
import { save } from './save.js';
import test from 'ava';

test('Simple', async (t) => {
  const graph = run(() => Box2(10, 20).color('blue').save('out'));
  t.deepEqual(await read('shape/out'), undefined);
});
