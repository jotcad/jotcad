import { describe, it } from 'node:test';
import { getShapesByTag, getTagValues, setTag } from './tag.js';

import assert from 'node:assert/strict';
import { makeShape } from './shape.js';

describe('tag', () => {
  it('should add a blue color tag', () =>
    assert.deepEqual(
      setTag(makeShape(), 'color', 'blue'),
      makeShape({ tags: { color: 'blue' } })
    ));

  it('should work on deep structures', () =>
    assert.deepEqual(
      setTag(
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
    ));

  it('should be readable with getTag', () => {
    const tagged = setTag(makeShape(), 'id', 'one');
    assert.deepEqual(tagged, makeShape({ tags: { id: 'one' } }));

    const shapes = getShapesByTag(tagged, 'id', 'one');
    assert.deepEqual(shapes, makeShape({ shapes: [tagged] }));

    const tags = getTagValues(shapes, 'id');
    assert.deepEqual(tags, ['one']);
  });
});
