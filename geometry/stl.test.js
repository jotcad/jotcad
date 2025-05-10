import { cgal, cgalIsReady } from './getCgal.js';
import { fromStl, toStl } from './stl.js';

import { makeShape } from './shape.js';
import test from 'ava';

test.beforeEach(async (t) => {
  await cgalIsReady;
});

const stlText = `solid JotCad
facet normal 0 0 1
  outer loop
    vertex 0 0 0
    vertex 1 0 0
    vertex 0 1 0
  endloop
endfacet
endsolid JotCad
`;

const geometryText = `v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 0 1 0 0 1 0
t 0 1 2
`;

test('to', (t) => {
  const assets = {
    text: {
      test: 'v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nt 0 1 2\n',
    },
  };
  t.is(
    stlText,
    new TextDecoder('utf8').decode(
      toStl(assets, makeShape({ geometry: 'test' }))
    )
  );
});

test('from', (t) => {
  const assets = { text: {} };
  const shape = fromStl(assets, new TextEncoder('utf8').encode(stlText));
  t.is(geometryText, assets.text[shape.geometry]);
});
