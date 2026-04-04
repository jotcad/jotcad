import { DecodeInexactGeometryText } from './geometry.js';
import { makeAbsolute } from './makeAbsolute.js';
import { toRgbFromTags } from './color.js';

const X = 0;
const Y = 1;
const Z = 2;

const toFillColor = (rgb) =>
  `${(rgb[0] / 255).toFixed(9)} ${(rgb[1] / 255).toFixed(9)} ${(
    rgb[2] / 255
  ).toFixed(9)} rg`;
const toStrokeColor = (rgb) =>
  `${(rgb[0] / 255).toFixed(9)} ${(rgb[1] / 255).toFixed(9)} ${(
    rgb[2] / 255
  ).toFixed(9)} RG`;

const black = [0, 0, 0];

const header = ({
  min,
  max,
  scale = 1,
  width = 210,
  height = 297,
  trim = 5,
  lineWidth = 0.096,
}) => {
  const mediaX1 = (min[X] - trim) * scale;
  const mediaY1 = (min[Y] - trim) * scale;
  const mediaX2 = (max[X] + trim) * scale;
  const mediaY2 = (max[Y] + trim) * scale;
  const trimX1 = min[X] * scale;
  const trimY1 = min[Y] * scale;
  const trimX2 = max[X] * scale;
  const trimY2 = max[Y] * scale;
  return [
    `%PDF-1.5`,
    `1 0 obj << /Pages 2 0 R /Type /Catalog >> endobj`,
    `2 0 obj << /Count 1 /Kids [ 3 0 R ] /Type /Pages >> endobj`,
    `3 0 obj <<`,
    `  /Contents 4 0 R`,
    `  /MediaBox [ ${mediaX1.toFixed(9)} ${mediaY1.toFixed(
      9
    )} ${mediaX2.toFixed(9)} ${mediaY2.toFixed(9)} ]`,
    `  /TrimBox [ ${trimX1.toFixed(9)} ${trimY1.toFixed(9)} ${trimX2.toFixed(
      9
    )} ${trimY2.toFixed(9)} ]`,
    `  /Parent 2 0 R`,
    `  /Type /Page`,
    `>>`,
    `endobj`,
    `4 0 obj << >>`,
    `stream`,
    `${lineWidth.toFixed(9)} w`,
  ];
};

const footer = [
  `endstream`,
  `endobj`,
  `trailer << /Root 1 0 R /Size 4 >>`,
  `%%EOF`,
];

export const toPdf = (
  assets,
  shape,
  { lineWidth = 0.096, size = [210, 297], definitions } = {}
) => {
  const absolute = makeAbsolute(assets, shape);
  const geometries = [];
  let min = [Infinity, Infinity, Infinity];
  let max = [-Infinity, -Infinity, -Infinity];

  const updateBoundingBox = (vertices) => {
    for (const [x, y, z] of vertices) {
      if (x < min[X]) min[X] = x;
      if (y < min[Y]) min[Y] = y;
      if (z < min[Z]) min[Z] = z;
      if (x > max[X]) max[X] = x;
      if (y > max[Y]) max[Y] = y;
      if (z > max[Z]) max[Z] = z;
    }
  };

  absolute.walk((shape, descend) => {
    if (shape.geometry) {
      const geometry = DecodeInexactGeometryText(
        assets.getText(shape.geometry)
      );
      updateBoundingBox(geometry.vertices);
      geometries.push({ tags: shape.tags, ...geometry });
    }
    descend();
  });

  if (geometries.length === 0) {
    min = [0, 0, 0];
    max = [0, 0, 0];
  }

  // This is the size of a post-script point in mm.
  const pointSize = 0.352777778;
  const scale = 1 / pointSize;
  const width = max[X] - min[X];
  const height = max[Y] - min[Y];
  const lines = [];

  for (const { tags, vertices, triangles, faces, segments } of geometries) {
    const fillColor = toFillColor(toRgbFromTags(tags, definitions, black));
    const strokeColor = toStrokeColor(toRgbFromTags(tags, definitions, black));

    const drawPolygon = (indices) => {
      indices.forEach((index, nth) => {
        const [x, y] = vertices[index];
        const sx = x * scale;
        const sy = y * scale;
        if (nth === 0) {
          lines.push(`${sx.toFixed(9)} ${sy.toFixed(9)} m`); // move-to.
        } else {
          lines.push(`${sx.toFixed(9)} ${sy.toFixed(9)} l`); // line-to.
        }
      });
      lines.push(`h`); // Polygons are always closed.
    };

    // Handle triangles (as filled polygons)
    for (const triangle of triangles) {
      lines.push(fillColor);
      lines.push(strokeColor);
      drawPolygon(triangle);
      lines.push(`f`);
    }

    // Handle faces with holes
    for (const [face, ...holes] of faces) {
      lines.push(fillColor);
      lines.push(strokeColor);
      drawPolygon(face);
      for (const hole of holes) {
        drawPolygon(hole);
      }
      lines.push(`f`);
    }

    // Handle segments
    if (segments.length > 0) {
      lines.push(strokeColor);
      let last;
      for (const [startIdx, endIdx] of segments) {
        const start = vertices[startIdx];
        const end = vertices[endIdx];
        const sx1 = start[X] * scale;
        const sy1 = start[Y] * scale;
        const sx2 = end[X] * scale;
        const sy2 = end[Y] * scale;

        if (!last || start[X] !== last[X] || start[Y] !== last[Y]) {
          if (last) {
            lines.push(`S`); // stroke.
          }
          lines.push(`${sx1.toFixed(9)} ${sy1.toFixed(9)} m`); // move-to.
        }
        lines.push(`${sx2.toFixed(9)} ${sy2.toFixed(9)} l`); // line-to.
        last = end;
      }
      lines.push(`S`); // stroke.
    }
  }

  const output = []
    .concat(
      header({ scale, min, max, width, height, lineWidth }),
      lines,
      footer
    )
    .join('\n');
  return new TextEncoder('utf8').encode(`${output}\n`);
};
