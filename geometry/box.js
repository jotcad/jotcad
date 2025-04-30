import { cgal, cgalIsReady } from './getCgal.js';

import { assets } from './assets.js';
import { shape } from './shape.js';
import { textId } from './assets.js';

export const Box = ([x0, y0, z0], [x1, y1, z1]) => {
  const text = `
v ${x0} ${y0} ${z0}
v ${x1} ${y0} ${z0}
v ${x0} ${y1} ${z0}
v ${x1} ${y1} ${z0}
v ${x0} ${y0} ${z1}
v ${x1} ${y0} ${z1}
v ${x0} ${y1} ${z1}
v ${x1} ${y1} ${z1}
t 7 3 2
t 2 6 7
t 7 6 4
t 4 5 7
t 7 5 1
t 1 3 7
t 0 2 3
t 3 1 0
t 0 1 5
t 5 4 0
t 6 2 0
t 0 4 6
`;

  return shape({ geometry: cgal.Triangulate(assets, textId(text)) });
};

`
t 4 5 7
t 7 6 4
t 5 1 3
t 3 7 5
t 2 6 7
t 3 2 7
t 1 5 4
t 4 0 1
t 1 0 2
t 1 2 3
t 4 6 2
t 2 0 4
`
