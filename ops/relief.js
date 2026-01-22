import './shapesSpec.js';
import './optionsSpec.js';
import './flagsSpec.js';
import './numberSpec.js';
import './stringSpec.js';
import { relief as geometryRelief } from '../geometry/relief.js';
import { registerOp } from './op.js';

export const Relief = registerOp({
  name: 'Relief',
  spec: [
    null,
    [
      'shapes',
      [
        'options',
        {
          rows: 'number',
          cols: 'number',
          mapping: 'string',
          subdivisions: 'number',
          closedU: 'boolean',
          closedV: 'boolean',
          targetEdgeLength: 'number',
        },
      ],
      ['flags', ['closedU', 'closedV']],
    ],
    'shape',
  ],
  code: (id, context, input, points, options = {}, flags = {}) =>
    geometryRelief(context.assets, points, { ...options, ...flags }),
});

export const relief = registerOp({
  name: 'relief',
  spec: [
    'shape',
    [
      'shapes',
      [
        'options',
        {
          rows: 'number',
          cols: 'number',
          mapping: 'string',
          subdivisions: 'number',
          closedU: 'boolean',
          closedV: 'boolean',
          targetEdgeLength: 'number',
        },
      ],
      ['flags', ['closedU', 'closedV']],
    ],
    'shape',
  ],
  code: (id, context, input, points, options = {}, flags = {}) =>
    geometryRelief(context.assets, [input, ...points], {
      ...options,
      ...flags,
    }),
});
