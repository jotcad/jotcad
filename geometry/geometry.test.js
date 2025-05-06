import { DecodeInexactGeometryText, EncodeInexactGeometryText } from './geometry.js';

import test from 'ava';

test("encode faces", t => {
  t.is('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n',
       EncodeInexactGeometryText({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[],"triangles":[],"faces":[[[0,1,2]]]}));
});

test("decode faces", t => {
  t.deepEqual({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[],"triangles":[],"faces":[[[0,1,2]]]},
              DecodeInexactGeometryText('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nf 0 1 2\n'));
});

test("encode triangles", t => {
  t.is('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nt 0 1 2\n',
       EncodeInexactGeometryText({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[],"triangles":[[0, 1, 2]],"faces":[]}));
});

test("decode triangles", t => {
  t.deepEqual({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[],"triangles":[[0, 1, 2]],"faces":[]},
              DecodeInexactGeometryText('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\nt 0 1 2\n'));
});

test("encode segments", t => {
  t.is('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\ns 0 1 1 2 2 0\n',
       EncodeInexactGeometryText({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[[0, 1], [1, 2], [2, 0]],"triangles":[],"faces":[]}));
});

test("decode segments", t => {
  t.deepEqual({"vertices":[[0,0,0],[1,0,0],[0,1,0]],"segments":[[0, 1], [1, 2], [2, 0]],"triangles":[],"faces":[]},
              DecodeInexactGeometryText('v 0 0 0 0 0 0\nv 1 0 0 1 0 0\nv 0 1 0 0 1 0\ns 0 1 1 2 2 0\n'));
});
