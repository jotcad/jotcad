import { shape } from './shape.js';
import { textId } from './assets.js';

export const Point = (x = 0, y = 0, z = 0) =>
  shape({ geometry: textId(`v ${x} ${y} ${z}\np 0`) });

export const Points = (...points) =>
  shape({ geometry: textId(`${points.map(([x, y, z]) => `v ${x} ${y} ${z}`).join('\n')}\np ${points.map((_, n) => `${n}`).join(' ')}`) });
