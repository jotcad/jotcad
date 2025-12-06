import './booleanSpec.js';
import './flagsSpec.js';
import './intervalSpec.js';
import './numberSpec.js';
import './numbersSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import './shapesSpec.js';
import './stringSpec.js';
import './vectorSpec.js';

import { Op } from '@jotcad/op';
import { footprint } from './footprint.js';
import { grow } from './grow.js';

export { And } from './and.js';
export { Arc2 } from './arc.js';
export { Box2 } from './box.js';
export { Box3 } from './box.js';
export { Jot } from './jot.js';
export { Link } from './link.js';
export { Loop, loop } from './loop.js';
export { Orb } from './orb.js';
export { Point } from './point.js';
export { Stl } from './stl.js';
export { Z } from './z.js';
export { Rule, rule } from './rule.js';

export { absolute } from './absolute.js';
export { and } from './and.js';
export { at } from './at.js';
export { color } from './color.js';
export { clip } from './clip.js';
export { cut } from './cut.js';
export { extrude2 } from './extrude.js';
export { extrude3 } from './extrude.js';
export { fill2 } from './fill.js';
export { fill3 } from './fill.js';
export { get } from './get.js';
export { join } from './join.js';
export { jot } from './jot.js';
export { link } from './link.js';
export { nth } from './nth.js';
export { mask } from './mask.js';
export { png } from './png.js';
export { readFile } from './fs.js';
export { registerOp } from './op.js';
export { revert } from './revert.js';
export { rx } from './rx.js';
export { ry } from './ry.js';
export { rz } from './rz.js';
export { save } from './save.js';
export { set } from './set.js';
export { simplify } from './simplify.js';
export { stl } from './stl.js';
export { test } from './test.js';
export { transform } from './transform.js';
export { writeFile } from './fs.js';
export { x } from './x.js';
export { y } from './y.js';
export { z } from './z.js';
export { footprint } from './footprint.js';
export { grow } from './grow.js';

export { withFs } from './fs.js';

export const constructors = [
  'And',
  'Arc2',
  'Box2',
  'Box3',
  'Jot',
  'Orb',
  'Point',
  'Stl',
  'Rule',
  'Z',
];

export const operators = [
  'absolute',
  'and',
  'at',
  'color',
  'clip',
  'cut',
  'extrude2',
  'extrude3',
  'fill2',
  'fill3',
  'get',
  'join',
  'jot',
  'mask',
  'nth',
  'png',
  'revert',
  'rx',
  'ry',
  'rz',
  'set',
  'simplify',
  'stl',
  'rule',
  'test',
  'transform',
  'x',
  'y',
  'z',
  'footprint',
  'grow',
];
