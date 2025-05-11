import * as api from './dist/jotcad-ops.js';

import { cgalIsReady, staticDisplay } from './dist/jotcad-geometry.js';
import { readFile } from './dist/jotcad-ops.js';
import { run } from './dist/jotcad-op.js';

export const demo = async () => {
  const ecmascript =
    `Box2([0, 5], [0, 5])
       .fill()
       .extrude(z(5))
       .color('blue')
       .cut([Box3([2, 5], [2, 5], [2, 5])])
       .and(Arc2(20, { give: 1 }).color('red'))
       .save('cli_test.json')
       .png('cli_test.png', [25, 25, 25])
       .stl('cli_test.stl');
    `;
  const assets = { text: {} };
  const evaluator = new Function(
    `{ ${Object.keys(api).join(', ')} }`,
    ecmascript
  );
  await cgalIsReady;
  const graph = await run(assets, () => evaluator(api));
  const data = await readFile('cli_test.png');
  const base64Image = data.toString('base64');
  const dataUri = `data:image/png;base64,${base64Image}`;
  const imgElement = document.createElement('img');
  imgElement.src = dataUri;
  document.body.appendChild(imgElement);
  console.log(JSON.stringify(assets));
};
