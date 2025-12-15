import { cgal } from './getCgal.js';
import assert from 'assert/strict';

export const join = (assets, shape, tools, selfTouchEnvelopeSize) => {
  assert.equal(
    typeof selfTouchEnvelopeSize,
    'number',
    'Expected selfTouchEnvelopeSize to be a number'
  );
  return shape.derive({
    geometry: cgal.Join(
      assets,
      shape,
      [shape, ...tools],
      selfTouchEnvelopeSize
    ),
    shapes: null,
  });
};
