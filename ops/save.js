import './shapeSpec.js';
import './stringSpec.js';
import { registerOp } from './op.js';
import { writeFile } from './fs.js';

export const save = registerOp({
  name: 'save',
  spec: ['shape', ['string'], 'shape'],
  code: async (id, session, input, path) => {
    // Changed assets to session
    await writeFile(
      session.filePath(path),
      JSON.stringify({ assets: session.assets, input })
    ); // Use session.filePath and session.assets
    return input;
  },
});
