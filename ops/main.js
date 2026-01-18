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
export { Curve } from './curve.js';
export { Disjoint } from './disjoint.js';
export { Hull } from './hull.js';
export { Jot } from './jot.js';
export { Link } from './link.js';
export { Loop } from './loop.js';
export { Orb } from './orb.js';
export { Point } from './point.js';
export { Rule } from './rule.js';
export { Stl } from './stl.js';
export { Sweep } from './sweep.js';
export { X } from './x.js';
export { Y } from './y.js';
export { Z } from './z.js';

export { absolute } from './absolute.js';
export { and } from './and.js';
export { at } from './at.js';
export { clean } from './clean.js';
export { color } from './color.js';
export { clip } from './clip.js';
export { clipOpen } from './clipOpen.js';
export { close3 } from './close.js';
export { curve } from './curve.js';
export { cut2 } from './cut.js';
export { cut3 } from './cut.js';
export { cutOpen } from './cutOpen.js';
export { disjoint } from './disjoint.js';
export { edges } from './edges.js';
export { extrude2 } from './extrude.js';
export { extrude3 } from './extrude.js';
export { fill2 } from './fill.js';
export { fill3 } from './fill.js';
export { gap } from './gap.js';
export { get } from './get.js';
export { hull } from './hull.js';
export { join } from './join.js';
export { jot } from './jot.js';
export { loop } from './loop.js';
export { link } from './link.js';
export { material } from './material.js';
export { nth } from './nth.js';
export { mask } from './mask.js';
export { png } from './png.js';
export { readFile } from './fs.js';
export { registerOp } from './op.js';
export { revert } from './revert.js';
export { rule } from './rule.js';
export { rx } from './rx.js';
export { ry } from './ry.js';
export { rz } from './rz.js';
export { save } from './save.js';
export { scale } from './scale.js';
export { set } from './set.js';
export { shell } from './shell.js';
export { simplify } from './simplify.js';
export { smooth } from './smooth.js';
export { stl } from './stl.js';
export { sweep } from './sweep.js';
export { test } from './test.js';
export { transform } from './transform.js';
export { writeFile } from './fs.js';
export { x } from './x.js';
export { y } from './y.js';
export { z } from './z.js';
export { footprint } from './footprint.js';
export { grow } from './grow.js';
export { wrap3 } from './wrap.js'; // MODIFIED: Export wrap3 from ops/wrap.js

export { withFs } from './fs.js';

export const constructors = [
  'And',
  'Arc2',
  'Box2',
  'Box3',
  'Curve',
  'Disjoint',
  'Hull',
  'Jot',
  'Link',
  'Loop',
  'Orb',
  'Point',
  'Stl',
  'Sweep',
  'Rule',
  'X',
  'Y',
  'Z',
];

export const operators = [
  'absolute',
  'and',
  'at',
  'clean',
  'color',
  'clip',
  'clipOpen',
  'close3',
  'curve',
  'cut2',
  'cut3',
  'cutOpen',
  'disjoint',
  'edges',
  'extrude2',
  'extrude3',
  'fill2',
  'fill3',
  'gap',
  'get',
  'hull',
  'join',
  'jot',
  'link',
  'material',
  'mask',
  'nth',
  'png',
  'revert',
  'rule',
  'rx',
  'ry',
  'rz',
  'scale',
  'set',
  'shell',
  'simplify',
  'smooth',
  'stl',
  'sweep',
  'test',
  'transform',
  'x',
  'y',
  'z',
  'footprint',
  'grow',
  'wrap3', // MODIFIED: 'wrap' to 'wrap3' in operators
];
