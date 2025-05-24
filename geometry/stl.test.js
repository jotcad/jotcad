import { cgal, cgalIsReady } from './getCgal.js';
import { fromStl, toStl } from './stl.js';

import { makeShape } from './shape.js';
import test from 'ava';
import { withAssets } from './assets.js';

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

const geometryText = `V 3
v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 0 1 0 0 1 0
T 1
t 2 0 1
`;

test('to', (t) =>
  withAssets(async (assets) => {
    const id = assets.textId(
      'V 3\nv 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nT 1\nt 0 1 2\n'
    );
    t.is(
      stlText,
      new TextDecoder('utf8').decode(toStl(assets, makeShape({ geometry: id })))
    );
  }));

test('from', (t) =>
  withAssets(async (assets) => {
    const shape = fromStl(assets, new TextEncoder('utf8').encode(stlText));
    t.is(geometryText, assets.getText(shape.geometry));
  }));
