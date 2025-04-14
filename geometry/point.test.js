import { Point, Points } from './point.js';
import { fromId, withGeometry } from './geometry.js';

import test from 'ava';

test('Point', t => {
  withGeometry(() => {
    t.deepEqual(Point(1, 2, 3),
      {
        geometry: ['acQ5TzAH5wRVbyo+JVR/0WOG78NDgiYNtp0Fb+Agwuk='],
        shapes: [],
        tags: {},
        tf: null,
      });
    t.deepEqual(fromId('acQ5TzAH5wRVbyo+JVR/0WOG78NDgiYNtp0Fb+Agwuk='),
                { points: [[ 1, 2, 3, ]], type: 'points', });
  });
});

test('Points', t => {
  withGeometry(() => {
    t.deepEqual(Points([[1, 2, 3], [4, 5, 6]]),
      {
        geometry: ['pGWoNPmpsLtM1pyBuc5WuFgSiwQ6E4JD7lxF1Uipcr8='],
        shapes: [],
        tags: {},
        tf: null,
      });
    t.deepEqual(fromId('pGWoNPmpsLtM1pyBuc5WuFgSiwQ6E4JD7lxF1Uipcr8='),
                { points: [[ 1, 2, 3, ], [4, 5, 6]], type: 'points', });
  });
});
