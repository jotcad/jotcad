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
  width = 210,
  height = 297,
  trim = 5,
  lineWidth = 0.096,
}) => {
  const mediaX2 = width + trim * 2;
  const mediaY2 = height + trim * 2;
  const trimX1 = trim;
  const trimY1 = trim;
  const trimX2 = width + trim;
  const trimY2 = height + trim;
  return [
    `%PDF-1.5`,
    `1 0 obj << /Pages 2 0 R /Type /Catalog >> endobj`,
    `2 0 obj << /Count 1 /Kids [ 3 0 R ] /Type /Pages >> endobj`,
    `3 0 obj <<`,
    `  /Contents 4 0 R`,
    `  /MediaBox [ 0 0 ${mediaX2.toFixed(9)} ${mediaY2.toFixed(9)} ]`,
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
  { lineWidth = 0.096, size = [210, 297], definitions, trim = 5 } = {}
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
  const width = (max[X] - min[X]) * scale;
  const height = (max[Y] - min[Y]) * scale;
  const lines = [];

  for (const { tags, vertices, triangles, faces, segments } of geometries) {
    const fillColor = toFillColor(toRgbFromTags(tags, definitions, black));
    const strokeColor = toStrokeColor(toRgbFromTags(tags, definitions, black));

    const drawPolygon = (indices) => {
      indices.forEach((index, nth) => {
        const [x, y] = vertices[index];
        // Translate to origin and add trim offset
        const sx = (x - min[X]) * scale + (trim * scale);
        const sy = (y - min[Y]) * scale + (trim * scale);
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
        const sx1 = (start[X] - min[X]) * scale + (trim * scale);
        const sy1 = (start[Y] - min[Y]) * scale + (trim * scale);
        const sx2 = (end[X] - min[X]) * scale + (trim * scale);
        const sy2 = (end[Y] - min[Y]) * scale + (trim * scale);

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
      header({ width, height, trim: trim * scale, lineWidth }),
      lines,
      footer
    )
    .join('\n');
  return new TextEncoder('utf8').encode(`${output}\n`);
};
