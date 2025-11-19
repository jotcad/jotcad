import {
  DecodeInexactGeometryText,
  EncodeInexactGeometryText,
} from './geometry.js';
import { cgal } from './getCgal.js';
import { makeAbsolute } from './makeAbsolute.js';
import { makeShape } from './shape.js';

export const fromJot = async (assets, id) => {
  const assetText = await assets.getText(id);
  if (!assetText) {
    throw new Error(`Jot asset with ID ${id} not found.`);
  }

  const { vertices, segments, triangles, faces } =
    DecodeInexactGeometryText(assetText);

  const geometry = new cgal.Geometry();

  if (vertices.length > 0) {
    geometry.ReserveVertices(vertices.length);
    for (const [x, y, z] of vertices) {
      geometry.AddVertexInexact(x, y, z, false);
    }
  }

  if (segments.length > 0) {
    geometry.ReserveSegments(segments.length);
    for (const [a, b] of segments) {
      geometry.AddSegment(a, b);
    }
  }

  if (triangles.length > 0) {
    geometry.ReserveTriangles(triangles.length);
    for (const [a, b, c] of triangles) {
      geometry.AddTriangle(a, b, c);
    }
  }

  if (faces.length > 0) {
    geometry.ReserveFaces(faces.length);
    for (const [face, holes] of faces) {
      geometry.AddFace(face);
      if (holes) {
        for (const hole of holes) {
          geometry.AddFaceHole(hole);
        }
      }
    }
  }

  // geometry.Repair(); // Removed as it might cause non-deterministic TextId
  return makeShape({ geometry: cgal.TextId(assets, geometry) });
};

export const toJot = async (assets, shape) => {
  const absolute = makeAbsolute(assets, shape);
  const allVertices = [];
  const allSegments = [];
  const allTriangles = [];
  const allFaces = [];

  absolute.walk((subShape, descend) => {
    if (subShape.geometry) {
      const { vertices, segments, triangles, faces } =
        DecodeInexactGeometryText(assets.getText(subShape.geometry));

      // Offset indices for aggregated geometry
      const vertexOffset = allVertices.length;
      const segmentOffset = allSegments.length;
      const triangleOffset = allTriangles.length;
      const faceOffset = allFaces.length;

      allVertices.push(...vertices);
      allSegments.push(
        ...segments.map(([a, b]) => [a + vertexOffset, b + vertexOffset])
      );
      allTriangles.push(
        ...triangles.map(([a, b, c]) => [
          a + vertexOffset,
          b + vertexOffset,
          c + vertexOffset,
        ])
      );
      allFaces.push(
        ...faces.map(([face, holes]) => [
          face.map((idx) => idx + vertexOffset),
          holes
            ? holes.map((hole) => hole.map((idx) => idx + vertexOffset))
            : undefined,
        ])
      );
    }
    descend();
  });

  const assetText = EncodeInexactGeometryText({
    vertices: allVertices,
    segments: allSegments,
    triangles: allTriangles,
    faces: allFaces,
  });

  return assets.textId(assetText); // Store the aggregated asset text and return its ID
};
