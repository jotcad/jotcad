import { cgal } from './getCgal.js';
import { shape } from './shape.js';

export const fill = (assets, shapes, holes = false) =>
  shape({ geometry: cgal.Fill(assets, shapes, holes) });
