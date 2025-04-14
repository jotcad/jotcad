import { shape } from './shape.js';
import { getId } from './geometry.js';

export const Point = (x = 0, y = 0, z = 0) =>
  shape({ geometry: [getId({ type: 'points', points: `${x} ${y} ${z}` })] });

export const Points = (points) =>
  shape({ geometry: [getId({ type: 'points', points: points.map(point => `${x} ${y} ${z}`).join(' ') })] });
