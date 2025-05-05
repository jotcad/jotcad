import './transform.js';

import { cgal, cgalIsReady } from './getCgal.js';

import { shape } from './shape.js';
import { textId } from './assets.js';

const unitBox3Text = `
v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 0 1 0 0 1 0
v 1 1 0 1 1 0
v 0 0 1 0 0 1
v 1 0 1 1 0 1
v 0 1 1 0 1 1
v 1 1 1 1 1 1
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

export const Box3 = (assets, [x0, y0, z0], [x1, y1, z1]) =>
  shape({ geometry: textId(assets, unitBox3Text) })
    .scale(x1 - x0, y1 - y0, z1 - z0)
    .move(-x0, -y0, -z0);

const unitBox2Text = `
v 0 0 0 0 0 0
v 1 0 0 1 0 0
v 1 1 0 1 1 0
v 0 1 0 0 1 0
s 0 1 1 2 2 3 3 0
`;

export const Box2 = (assets, [x0, y0], [x1, y1]) =>
  shape({ geometry: textId(assets, unitBox2Text) })
    .scale(x1 - x0, y1 - y0)
    .move(-x0, -y0);
