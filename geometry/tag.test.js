import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { makeShape } from './shape.js';
import { tag } from './tag.js';

describe('tag', () => {
  it('should add a blue color tag', () =>
    assert.deepEqual(
      tag(makeShape(), 'color', 'blue'),
      makeShape({ tags: { color: 'blue' } })
    ));

  it('should work on deep structures', () =>
    assert.deepEqual(
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
    ));
});
