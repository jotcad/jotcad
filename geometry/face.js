import { makeShape } from './shape.js';

const buildFaceText = (points) =>
  `${points.map(([x, y, z]) => `v ${x} ${y} ${z}`).join('\n')}\nf ${points
    .map((_, n) => `${n}`)
    .join(' ')}\n`;

const buildHoleText = (points) =>
  `${points.map(([x, y, z]) => `v ${x} ${y} ${z}`).join('\n')}\nh ${points
    .map((_, n) => `${n}`)
    .join(' ')}\n`;

const buildHolesText = (holes) =>
  holes.map((hole) => buildHoleText(hole)).join('\n');

const buildFaceWithHolesText = (points, holes) =>
  buildFaceText(points) + buildHolesText(holes);

export const Face = (assets, points, holes = []) =>
  makeShape({
    geometry: assets.textId(buildFaceWithHolesText(points, holes)),
  });
