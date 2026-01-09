import './shapeSpec.js';
import './numberSpec.js';
import './booleanSpec.js';
import './optionsSpec.js';
import { shell as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const shell = registerOp({
  name: 'shell',
  spec: [
    'shape',
    [
      'number',
      'number',
      'boolean',
      'number',
      'number',
      'number',
      'number',
      [
        'options',
        {
          innerOffset: 'number',
          outerOffset: 'number',
          protect: 'boolean',
          angle: 'number',
          sizing: 'number',
          approx: 'number',
          edgeSize: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (
    id,
    session,
    input,
    implicitInnerOffset = 0,
    implicitOuterOffset = 1,
    implicitProtect = false,
    implicitAngle = 30,
    implicitSizing = 1,
    implicitApprox = 0.1,
    implicitEdgeSize = 1,
    {
      innerOffset = implicitInnerOffset,
      outerOffset = implicitOuterOffset,
      protect = implicitProtect,
      angle = implicitAngle,
      sizing = implicitSizing,
      approx = implicitApprox,
      edgeSize = implicitEdgeSize,
    } = {}
  ) =>
    op(
      session.assets,
      input,
      innerOffset,
      outerOffset,
      protect,
      angle,
      sizing,
      approx,
      edgeSize
    ),
});
