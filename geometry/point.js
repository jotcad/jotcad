import { shape } from './shape.js';
import { textId } from './assets.js';

export const Point = (assets, x = 0, y = 0, z = 0, ix = x, iy = y, iz = z) =>
  shape({
    geometry: textId(assets, `v ${x} ${y} ${z} ${ix} ${iy} ${iz}\np 0`),
  });

export const Points = (assets, points) =>
  shape({
    geometry: textId(
      assets,
      `${points
        .map(
          ([x, y, z, ix = x, iy = y, iz = z]) =>
            `v ${x} ${y} ${z} ${ix} ${iy} ${iz}`
        )
        .join('\n')}\np ${points.map((_, n) => `${n}`).join(' ')}`
    ),
  });
