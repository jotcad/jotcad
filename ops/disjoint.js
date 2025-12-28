import './shapesSpec.js';
import './shapeSpec.js';
import { disjoint as disjointGeom } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const Disjoint = registerOp({
  name: 'Disjoint',
  spec: [null, ['shapes'], 'shape'],
  code: (id, session, input, shapes) => disjointGeom(session.assets, shapes),
});

export const disjoint = registerOp({
  name: 'disjoint',
  spec: ['shape', ['shapes'], 'shape'],
  code: (id, session, input, shapes) =>
    disjointGeom(session.assets, [input, ...shapes]),
});
