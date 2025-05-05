import { shape } from './shape.js';
import { tag } from './tag.js';
import test from 'ava';

test('tag leaf', (t) => {
  t.deepEqual(
    tag(shape(), 'color', 'blue'),
    shape({ tags: { color: 'blue' } })
  );
});

test('tag branch', (t) => {
  t.deepEqual(
    tag(
      shape({ shapes: [shape({ tags: { material: 'copper' } }), shape()] }),
      'color',
      'blue'
    ),
    shape({
      shapes: [
        shape({ tags: { color: 'blue', material: 'copper' } }),
        shape({ tags: { color: 'blue' } }),
      ],
      tags: { color: 'blue' },
    })
  );
});
