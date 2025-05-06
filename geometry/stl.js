import {
  DecodeInexactGeometryText,
  EncodeInexactGeometryText,
} from './geometry.js';

import parseStlAscii from 'parse-stl-ascii';
import { parse as parseStlBinary } from './parseStlBinary.js';
import { shape } from './shape.js';
import { textId } from './assets.js';

const parse = (data, format) => {
  switch (format) {
    case 'ascii':
      try {
        return parseStlAscii(new TextDecoder('utf8').decode(data));
      } catch (error) {
        if (error instanceof RangeError) {
          return parseStlBinary(data);
        } else {
          throw error;
        }
      }
    case 'binary':
      try {
        return parseStlBinary(data);
      } catch (error) {
        // Try falling back to ascii.
        if (error instanceof RangeError) {
          return parseStlAscii(new TextDecoder('utf8').decode(data));
        } else {
          throw error;
        }
      }
    default:
      throw Error('die');
  }
};

export const fromStl = (assets, stl, { format = 'ascii' } = {}) => {
  const { positions, cells } = parse(stl, format);
  const text = EncodeInexactGeometryText({
    vertices: positions,
    triangles: cells,
  });
  return shape({ geometry: textId(assets, text) });
};

const X = 0;
const Y = 1;
const Z = 2;

const subtract = ([ax, ay, az], [bx, by, bz]) => [ax - bx, ay - by, az - bz];

const computeNormal = ([one, two, three]) => {
  const [aX, aY, aZ] = subtract(two, one);
  const [bX, bY, bZ] = subtract(three, one);
  const nX = aY * bZ - aZ * bY;
  const nY = aZ * bX - aX * bZ;
  const nZ = aX * bY - aY * bX;
  const length = Math.sqrt(nX * nX + nY * nY + nZ * nZ);
  return [nX / length, nY / length, nZ / length];
};

const equals = ([aX, aY, aZ], [bX, bY, bZ]) =>
  aX === bX && aY === bY && aZ === bZ;

// const round = (value, tolerance) => Math.round(value / tolerance) * tolerance;

// Do not round.
const round = (value, tolerance) => value;

const roundVertex = ([x, y, z], tolerance = 0.001) => [
  round(x, tolerance),
  round(y, tolerance),
  round(z, tolerance),
];

const convertToFacets = (polygons) =>
  polygons
    .map(convertToFacet)
    .filter((facet) => facet !== undefined)
    .join('\n');

const toStlVector = (vector) => `${vector[0]} ${vector[1]} ${vector[2]}`;

const toStlVertex = (vertex) => `vertex ${toStlVector(vertex)}`;

const convertToFacet = (polygon) => {
  if (polygon.length !== 3) {
    throw Error(`die`);
  }
  if (
    equals(polygon[0], polygon[1]) ||
    equals(polygon[1], polygon[2]) ||
    equals(polygon[2], polygon[0])
  ) {
    // Filter degenerate facets.
    return;
  }
  const normal = computeNormal(polygon);
  if (normal !== undefined) {
    return (
      `facet normal ${toStlVector(normal)}\n` +
      `  outer loop\n` +
      `    ${toStlVertex(polygon[0])}\n` +
      `    ${toStlVertex(polygon[1])}\n` +
      `    ${toStlVertex(polygon[2])}\n` +
      `  endloop\n` +
      `endfacet`
    );
  }
};

// We sort the triangles to produce stable output.
const orderVertices = (v) => {
  let min = v[0];
  for (let d = 1; d < 3; d++) {
    const c = v[d];
    const dX = min[X] - c[X];
    if (dX < 0) {
      continue;
    } else if (dX === 0) {
      const dY = min[Y] - c[Y];
      if (dY < 0) {
        continue;
      } else if (dY === 0) {
        const dZ = min[Z] - c[Z];
        if (dZ < 0) {
          continue;
        }
      }
    }
    min = c;
  }
  while (v[0] !== min) {
    v.push(v.shift());
  }
  return v;
};

const compareTriangles = (t1, t2) => {
  // The triangle vertices have been ordered such that the top is the minimal vertex.
  for (let d = 0; d < 3; d++) {
    const a = t1[d];
    const b = t2[d];
    const dX = a[X] - b[X];
    if (dX < 0) {
      return -1;
    } else if (dX === 0) {
      const dY = a[Y] - b[Y];
      if (dY < 0) {
        return -1;
      } else if (dY === 0) {
        const dZ = a[Z] - b[Z];
        if (dZ < 0) {
          return -1;
        } else if (dZ === 0) {
          continue;
        }
      }
    }
    return 1;
  }
  return 0;
};

export const toStl = (assets, shape, tolerance = 0.001) => {
  const stlTriangles = [];
  shape.walk((shape, descend) => {
    if (shape.geometry) {
      const { vertices, triangles } = DecodeInexactGeometryText(
        assets.text[shape.geometry]
      );
      for (const [a, b, c] of triangles) {
        stlTriangles.push(
          orderVertices([
            roundVertex(vertices[a], tolerance),
            roundVertex(vertices[b], tolerance),
            roundVertex(vertices[c], tolerance),
          ])
        );
      }
    }
    descend();
  });
  stlTriangles.sort(compareTriangles);
  const output = `solid JotCad\n${convertToFacets(
    stlTriangles
  )}\nendsolid JotCad\n`;
  return new TextEncoder('utf8').encode(output);
};
