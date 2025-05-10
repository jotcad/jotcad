import { makeShape } from './shape.js';
import { tag } from './tag.js';
import test from 'ava';

test('tag leaf', (t) => {
  t.deepEqual(
    tag(makeShape(), 'color', 'blue'),
    makeShape({ tags: { color: 'blue' } })
  );
});

test('tag branch', (t) => {
  t.deepEqual(
    tag(
      makeShape({
        shapes: [makeShape({ tags: { material: 'copper' } }), makeShape()],
      }),
      'color',
      'blue'
    ),
    makeShape({
      shapes: [
        makeShape({ tags: { color: 'blue', material: 'copper' } }),
        makeShape({ tags: { color: 'blue' } }),
      ],
      tags: { color: 'blue' },
    })
  );
});
