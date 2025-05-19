import { makeShape } from './shape.js';

export const Point = (assets, x = 0, y = 0, z = 0, ix = x, iy = y, iz = z) =>
  makeShape({
    geometry: assets.textId(`v ${x} ${y} ${z} ${ix} ${iy} ${iz}\np 0`),
  });

export const Points = (assets, points) =>
  makeShape({
    geometry: assets.textId(
      `${points
        .map(
          ([x, y, z, ix = x, iy = y, iz = z]) =>
            `v ${x} ${y} ${z} ${ix} ${iy} ${iz}`
        )
        .join('\n')}\np ${points.map((_, n) => `${n}`).join(' ')}`
    ),
  });
