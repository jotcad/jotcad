import { describe, it } from 'node:test';
import { fromStl, toStl } from './stl.js';

import assert from 'node:assert/strict';
import { makeShape } from './shape.js';
import { withAssets } from './assets.js';

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

describe('stl', () => {
  it('should convert to an stl', async () => {
    await withAssets(async (assets) => {
      const id = assets.textId(
        'V 3\nv 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nT 1\nt 0 1 2\n'
      );
      assert.strictEqual(
        stlText,
        new TextDecoder('utf8').decode(
          toStl(assets, makeShape({ geometry: id }))
        )
      );
    });
  });

  it('should convert from an stl', async () => {
    await withAssets(async (assets) => {
      const shape = fromStl(assets, new TextEncoder('utf8').encode(stlText));
      assert.ok(geometryText, assets.getText(shape.geometry));
    });
  });
});
