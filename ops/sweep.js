import { sweep as geometrySweep } from '../geometry/sweep.js';
import { registerOp } from './op.js';

const STRATEGY_MAP = {
  simple: 0,
  miter: 1,
  round: 2,
};

const resolveOptions = (options) => {
  const {
    closedPath = false,
    closedProfile = false,
    strategy = 'simple',
    solid = true,
    iota = -1,
    minTurnRadius = 0.01,
  } = options || {};
  return {
    closedPath,
    closedProfile,
    strategy: STRATEGY_MAP[strategy] ?? 0,
    solid,
    iota,
    minTurnRadius: Math.max(minTurnRadius, 0.0001),
  };
};

export const Sweep = registerOp({
  name: 'Sweep',
  spec: [
    null,
    [
      'shape',
      'shapes',
      [
        'options',
        {
          closedPath: 'boolean',
          closedProfile: 'boolean',
          strategy: 'string',
          solid: 'boolean',
          iota: 'number',
          minTurnRadius: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (id, context, input, path, profiles, options) =>
    geometrySweep(context.assets, profiles, path, resolveOptions(options)),
});

export const sweep = registerOp({
  name: 'sweep',
  spec: [
    'shape',
    [
      'shapes',
      [
        'options',
        {
          closedPath: 'boolean',
          closedProfile: 'boolean',
          strategy: 'string',
          solid: 'boolean',
          iota: 'number',
          minTurnRadius: 'number',
        },
      ],
    ],
    'shape',
  ],
  code: (id, context, input, profiles, options) =>
    geometrySweep(context.assets, profiles, input, resolveOptions(options)),
});
