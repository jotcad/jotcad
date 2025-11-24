import './numberSpec.js';
import './optionsSpec.js';
import './shapeSpec.js';
import { simplify as op } from '@jotcad/geometry';
import { registerOp } from './op.js';

export const simplify = registerOp({
  name: 'simplify',
  spec: [null, ['number', ['options', { faceCount: 'number' }]], 'shape'],
  code: (
    id,
    session, // Changed assets to session
    input,
    implicitFaceCount = 10000,
    { faceCount = implicitFaceCount } = {}
  ) => op(session.assets, input, { faceCount }), // Use session.assets
});
