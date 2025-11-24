// web/jot_threejs_viewer.js

import { DecodeInexactGeometryText } from './jot_parser.js';

// --- Copied and adapted from geometry/threejs.js ---
import * as THREE from 'three'; // Import THREE explicitly

const {
  BufferGeometry,
  Color,
  EdgesGeometry,
  Float32BufferAttribute,
  LineBasicMaterial,
  LineSegments,
  Matrix4,
  Mesh,
  MeshNormalMaterial,
  MeshStandardMaterial,
  Object3D,
  Plane,
  Vector3,
} = THREE;

const GEOMETRY_LAYER = 0;
const SKETCH_LAYER = 1;

const identityMatrix = new Matrix4();

const makeTranslateMatrix = (translateX, translateY, translateZ) => {
  const translationMatrix = new Matrix4();
  translationMatrix.set(
    1,
    0,
    0,
    translateX,
    0,
    1,
    0,
    translateY,
    0,
    0,
    1,
    translateZ,
    0,
    0,
    0,
    1
  );
  return translationMatrix;
};

const makeScaleMatrix = (scaleX, scaleY, scaleZ) => {
  const scaleMatrix = new Matrix4();
  scaleMatrix.set(
    scaleX,
    0,
    0,
    0,
    0,
    scaleY,
    0,
    0,
    0,
    0,
    scaleZ,
    0,
    0,
    0,
    0,
    1
  );
  return scaleMatrix;
};

const makeRotateXMatrix = (angle) => {
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  const matrix = new Matrix4();
  matrix.set(1, 0, 0, 0, 0, c, -s, 0, 0, s, c, 0, 0, 0, 0, 1);
  return matrix;
};

const makeRotateYMatrix = (angle) => {
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  const matrix = new Matrix4();
  matrix.set(c, 0, s, 0, 0, 1, 0, 0, -s, 0, c, 0, 0, 0, 0, 1);
  return matrix;
};

const makeRotateZMatrix = (angle) => {
  const c = Math.cos(angle);
  const s = Math.sin(angle);
  const matrix = new Matrix4();
  matrix.set(c, -s, 0, 0, s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  return matrix;
};

export const decodeTf = (tf) => {
  if (tf === undefined) {
    return identityMatrix;
  } else if (typeof tf === 'string') {
    const pieces = tf.split(' ');
    switch (pieces.shift()) {
      case 'x': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        return makeRotateXMatrix(Math.PI * 2 * tau);
      }
      case 'y': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        return makeRotateYMatrix(Math.PI * 2 * tau);
      }
      case 'z': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        return makeRotateZMatrix(Math.PI * 2 * tau);
      }
      case 's': {
        const x = parseFloat(pieces.shift());
        const y = parseFloat(pieces.shift());
        const z = parseFloat(pieces.shift());
        const sm = makeScaleMatrix(x, y, z);
        return sm;
      }
      case 't': {
        const x = parseFloat(pieces.shift());
        const y = parseFloat(pieces.shift());
        const z = parseFloat(pieces.shift());
        const tm = makeTranslateMatrix(x, y, z);
        return tm;
      }
      case 'm': {
        const matrix = new Matrix4();
        pieces.splice(0, 13);
        const a = parseFloat(pieces.shift()); // 11
        const b = parseFloat(pieces.shift()); // 12
        const c = parseFloat(pieces.shift()); // 13
        const d = parseFloat(pieces.shift()); // 14
        const e = parseFloat(pieces.shift()); // 21
        const f = parseFloat(pieces.shift()); // 22
        const g = parseFloat(pieces.shift()); // 23
        const h = parseFloat(pieces.shift()); // 24
        const i = parseFloat(pieces.shift()); // 31
        const j = parseFloat(pieces.shift()); // 32
        const k = parseFloat(pieces.shift()); // 33
        const l = parseFloat(pieces.shift()); // 34
        const m = parseFloat(pieces.shift()); // 44
        matrix.set(a, b, c, d, e, f, g, h, i, j, k, l, 0, 0, 0, m);
        return matrix;
      }
      case 'i': {
        // The inverse of the identity matrix is the identity matrix.
        return identityMatrix;
      }
      default: {
        throw Error(`Undecoded tf: ${tf}`);
      }
    }
  } else if (tf instanceof Array) {
    const b = decodeTf(tf[1]);
    if (tf[0] === 'i') {
      return b.clone().invert();
    } else {
      const a = decodeTf(tf[0]);
      return a.clone().multiply(b);
    }
  } else {
    throw Error(`Undecoded tf: ${tf}`);
  }
};

const triangulateFaceWithHoles = (points, face, holes) => {
  const vertices2D = [];
  const holeIndices = [];

  // Flatten the outer loop vertices
  for (const vertexIndex of face) {
    const point = points[vertexIndex];
    vertices2D.push(point[0], point[1]);
  }

  // Flatten the hole vertices and record their start indices
  for (const hole of holes) {
    holeIndices.push(vertices2D.length / 2); // Start index of the hole
    for (const vertexIndex of hole) {
      const point = points[vertexIndex];
      vertices2D.push(point[0], point[1]);
    }
  }

  // Use earcut to triangulate
  const triangles = earcut(vertices2D, holeIndices, 2); // 2 for 2D points
  return triangles;
};

const buildMeshMaterial = (shape) => {
  const { color } = shape.tags || {};
  if (color === undefined) {
    return new MeshNormalMaterial();
  } else {
    return new MeshStandardMaterial({ color });
  }
};

const buildLineMaterial = (shape) => {
  const { color, linewidth = 2 } = shape.tags || {};
  if (color === undefined) {
    return new LineBasicMaterial({ color: new Color('black'), linewidth: 2 });
  } else {
    return new LineBasicMaterial({ color: new Color(color), linewidth: 2 });
  }
};

const buildEdges = (geometry) => {
  const edges = new EdgesGeometry(geometry);
  const lines = new LineSegments(
    edges,
    new LineBasicMaterial({
      color: 0x000000,
      linewidth: 2,
    })
  );
  return lines;
};

// --- Main rendering function ---
export const renderJotToThreejsScene = async (jotText, scene) => {
  const geometryCache = new Map(); // Map TextId to THREE.BufferGeometry
  let mainShapeJson = null;

  const deserializedAssets = new Map();
  let offset = 0;

  // Parse JOT assetText
  if (jotText[offset] === '\n') {
    offset++;
  }

  while (offset < jotText.length) {
    if (jotText[offset] !== '=') {
      throw new Error(
        `Expected '=' at offset ${offset} but got '${jotText[offset]}'`
      );
    }
    offset++;

    const headerLineEnd = jotText.indexOf('\n', offset);
    if (headerLineEnd === -1) {
      throw new Error(`Expected newline after header at offset ${offset}`);
    }
    const headerLine = jotText.substring(offset, headerLineEnd);
    offset = headerLineEnd + 1;

    const firstSpace = headerLine.indexOf(' ');
    if (firstSpace === -1) {
      throw new Error(`Expected space in header line '${headerLine}'`);
    }

    const fileContentLengthStr = headerLine.substring(0, firstSpace);
    const fileContentLength = parseInt(fileContentLengthStr, 10);
    if (isNaN(fileContentLength)) {
      throw new Error(
        `Invalid content length in header: '${fileContentLengthStr}'`
      );
    }

    const fileName = headerLine.substring(firstSpace + 1);
    const fileContent = jotText.substring(offset, offset + fileContentLength);
    offset += fileContentLength;

    deserializedAssets.set(fileName, fileContent);

    if (offset < jotText.length && jotText[offset] === '\n') {
      offset++;
    }
  }

  // Identify main Shape JSON and geometry assets
  for (const [fileName, fileContent] of deserializedAssets.entries()) {
    if (fileName.startsWith('files/')) {
      if (mainShapeJson !== null) {
        throw new Error(
          'Multiple main Shape files found in JOT serialization.'
        );
      }
      mainShapeJson = JSON.parse(fileContent);
    } else if (fileName.startsWith('assets/text/')) {
      const geometryId = fileName.substring('assets/text/'.length);
      const { vertices, segments, triangles, faces } =
        DecodeInexactGeometryText(fileContent);

      // Convert raw geometry to Three.js BufferGeometry
      const bufferGeometry = new BufferGeometry();
      const positions = [];
      const indices = [];

      // Handle segments (lines)
      if (segments.length > 0) {
        const positions = [];
        for (const vertex of vertices) {
          positions.push(vertex[0], vertex[1], vertex[2]);
        }
        bufferGeometry.setAttribute(
          'position',
          new Float32BufferAttribute(positions, 3)
        );

        const indices = [];
        for (const [source, target] of segments) {
          indices.push(source, target);
        }
        bufferGeometry.setIndex(indices);
      }
      // Handle faces (polygons with holes)
      else if (faces.length > 0) {
        for (const [face, ...holes] of faces) {
          // Collect all vertices for the face (outer loop + holes)
          const faceVertices = [];
          const faceVertexMap = new Map(); // Map original vertex index to local faceVertices index

          const addVertexToFace = (vIdx) => {
            if (!faceVertexMap.has(vIdx)) {
              faceVertexMap.set(vIdx, vertices[vIdx]); // Store the actual 3D point
            }
            return faceVertexMap.get(vIdx);
          };

          const outerLoopPoints = face.map((vIdx) => vertices[vIdx]);
          const holeLoopPoints = holes.map((hole) =>
            hole.map((vIdx) => vertices[vIdx])
          );

          // Triangulate the 2D projection of the face
          // earcut expects a flat array of [x, y, x, y, ...]
          const flatVertices2D = [];
          const holeIndices = [];

          // Add outer loop vertices
          for (const point of outerLoopPoints) {
            flatVertices2D.push(point[0], point[1]);
          }

          // Add hole loop vertices and record their start indices
          for (const hole of holeLoopPoints) {
            holeIndices.push(flatVertices2D.length / 2);
            for (const point of hole) {
              flatVertices2D.push(point[0], point[1]);
            }
          }

          const triangulationIndices = earcut(flatVertices2D, holeIndices, 2);

          // Now map the 2D triangulation back to 3D vertices and compute normals
          // We need to reconstruct the original 3D vertices based on the flattened 2D array
          const allFacePoints = [...outerLoopPoints];
          for (const hole of holeLoopPoints) {
            allFacePoints.push(...hole);
          }

          // Compute normals for the face (assuming planar)
          let faceNormal = new Vector3();
          if (allFacePoints.length >= 3) {
            const v0 = new Vector3(...allFacePoints[0]);
            const v1 = new Vector3(...allFacePoints[1]);
            const v2 = new Vector3(...allFacePoints[2]);
            const cb = new Vector3().subVectors(v2, v1);
            const ab = new Vector3().subVectors(v0, v1);
            cb.cross(ab).normalize();
            faceNormal = cb;
          }

          for (const index of triangulationIndices) {
            const p = allFacePoints[index];
            positions.push(p[0], p[1], p[2]);
            normals.push(faceNormal.x, faceNormal.y, faceNormal.z);
          }
        }
      }
      // Handle triangles
      else if (triangles.length > 0) {
        const positions = [];
        const normals = [];
        for (const vertex of vertices) {
          positions.push(vertex[0], vertex[1], vertex[2]);
          normals.push(vertex[3], vertex[4], vertex[5]);
        }
        bufferGeometry.setAttribute(
          'position',
          new Float32BufferAttribute(positions, 3)
        );
        bufferGeometry.setAttribute(
          'normal',
          new Float32BufferAttribute(normals, 3)
        );

        const indices = [];
        for (const triangle of triangles) {
          indices.push(...triangle);
        }
        bufferGeometry.setIndex(indices);
      }

      geometryCache.set(geometryId, bufferGeometry);
    }
  }

  if (!mainShapeJson) {
    throw new Error(
      'No main Shape file found in JOT serialization (expected a file starting with "files/").'
    );
  }

  // Recursively build Three.js objects
  const walkShape = (shape, parentObject) => {
    const currentObject = new Object3D(); // Use a Group or Object3D for transformations
    parentObject.add(currentObject);

    if (shape.tf) {
      currentObject.applyMatrix4(decodeTf(shape.tf));
    }

    if (shape.geometry) {
      const geometryId = shape.geometry;
      const bufferGeometry = geometryCache.get(geometryId);
      if (bufferGeometry) {
        const material = buildMeshMaterial(shape); // Use buildMeshMaterial/buildLineMaterial
        let mesh;
        if (bufferGeometry.index && bufferGeometry.index.count > 0) {
          mesh = new Mesh(bufferGeometry, material);
        } else {
          // If no indices, assume it's lines or non-indexed triangles
          // This needs more robust handling based on geometry type
          mesh = new LineSegments(bufferGeometry, buildLineMaterial(shape));
        }
        currentObject.add(mesh);
        // Add edges if desired (buildEdges function)
        // const edges = buildEdges(bufferGeometry);
        // currentObject.add(edges);
      } else {
        console.warn(`Geometry with ID ${geometryId} not found in cache.`);
      }
    }

    if (shape.shapes) {
      for (const subShape of shape.shapes) {
        walkShape(subShape, currentObject);
      }
    }
  };

  walkShape(mainShapeJson, scene);

  return scene;
};
