/* global OffscreenCanvas */

import {
  AlwaysDepth,
  AxesHelper,
  BufferGeometry,
  Color,
  EdgesGeometry,
  Float32BufferAttribute,
  Layers,
  LessEqualDepth,
  LineBasicMaterial,
  LineSegments,
  Matrix4,
  Mesh,
  MeshNormalMaterial,
  MeshStandardMaterial,
  Object3D,
  PCFShadowMap,
  PerspectiveCamera,
  Plane,
  Scene,
  SpotLight,
  Vector3,
  WebGLRenderer,
} from '@jotcad/threejs';

import { DecodeInexactGeometryText } from './geometry.js';

import earcut from 'earcut';

export const GEOMETRY_LAYER = 0;
export const SKETCH_LAYER = 1;

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
        matrix.rotateX(Math.PI * 2 * tau);
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
        pieces.shift();
        pieces.shift();
        pieces.shift();
        const x = parseFloat(pieces.shift());
        const y = parseFloat(pieces.shift());
        const z = parseFloat(pieces.shift());
        const sm = makeScaleMatrix(x, y, z);
        return sm;
      }
      case 't': {
        pieces.shift();
        pieces.shift();
        pieces.shift();
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
  const vertices = [];
  const holeIndices = [];
  for (const vertex of face) {
    const point = points[vertex];
    vertices.push(point[0], point[1]);
  }
  for (const hole of holes) {
    holeIndices.push(vertices.length / 2);
    for (const vertex of hole) {
      const point = points[vertex];
      vertices.push(point[0], point[1]);
    }
  }
  const triangles = earcut(vertices, holeIndices);
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

export const buildMeshes = async ({
  assets,
  shape,
  scene,
  render,
  definitions,
  pageSize = [],
  doOutlineEdges = true,
}) => {
  const walk = async (shape) => {
    try {
      if (shape.geometry) {
        const matrix = decodeTf(shape.tf);
        const { vertices, segments, triangles, faces } =
          DecodeInexactGeometryText(assets.getText(shape.geometry));
        if (segments.length > 0) {
          const bufferGeometry = new BufferGeometry();
          const positions = [];
          for (const [source, target] of segments) {
            positions.push(...vertices[source], ...vertices[target]);
          }
          bufferGeometry.setAttribute(
            'position',
            new Float32BufferAttribute(positions, 3)
          );
          const material = buildLineMaterial(shape);
          const mesh = new LineSegments(bufferGeometry, material);
          mesh.applyMatrix4(matrix);
          scene.add(mesh);
        }
        if (faces.length > 0) {
          for (const [face, ...holes] of faces) {
            const triangulationIndices = triangulateFaceWithHoles(
              vertices,
              face,
              holes
            );
            const geometry = new BufferGeometry();
            const verticesArray = [];
            for (const vertex of face) {
              verticesArray.push(...vertices[vertex]);
            }
            for (const hole of holes) {
              for (const vertex of hole) {
                verticesArray.push(...vertices[vertex]);
              }
            }
            geometry.setAttribute(
              'position',
              new Float32BufferAttribute(verticesArray, 3)
            );
            const adjustedIndices = [];
            for (let i = 0; i < triangulationIndices.length; i += 3) {
              adjustedIndices.push(
                triangulationIndices[i],
                triangulationIndices[i + 1],
                triangulationIndices[i + 2]
              );
            }
            geometry.setIndex(adjustedIndices);
            const material = buildMeshMaterial(shape);
            const mesh = new Mesh(geometry, material);
            mesh.applyMatrix4(matrix);
            scene.add(mesh);
            if (doOutlineEdges) {
              const edges = buildEdges(geometry);
              edges.applyMatrix4(matrix);
              scene.add(edges);
            }
          }
        }
        if (triangles.length > 0) {
          const positions = [];
          const normals = [];
          for (const triangle of triangles) {
            const plane = new Plane();
            plane.setFromCoplanarPoints(
              new Vector3(...vertices[triangle[0]]),
              new Vector3(...vertices[triangle[1]]),
              new Vector3(...vertices[triangle[2]])
            );
            positions.push(...vertices[triangle[0]]);
            positions.push(...vertices[triangle[1]]);
            positions.push(...vertices[triangle[2]]);
            plane.normalize();
            const { x, y, z } = plane.normal;
            normals.push(x, y, z);
            normals.push(x, y, z);
            normals.push(x, y, z);
          }
          const bufferGeometry = new BufferGeometry();
          bufferGeometry.setAttribute(
            'position',
            new Float32BufferAttribute(positions, 3)
          );
          bufferGeometry.setAttribute(
            'normal',
            new Float32BufferAttribute(normals, 3)
          );
          const material = buildMeshMaterial(shape);
          const mesh = new Mesh(bufferGeometry, material);
          mesh.applyMatrix4(matrix);
          scene.add(mesh);
          if (doOutlineEdges) {
            const edges = buildEdges(bufferGeometry);
            edges.applyMatrix4(matrix);
            scene.add(edges);
          }
        }
      }

      if (shape.shapes) {
        try {
          for (const subShape of shape.shapes) {
            walk(subShape);
          }
        } catch (e) {
          console.log(JSON.stringify(shape));
          throw e;
        }
      }
    } catch (e) {
      console.log(`QQ/shape: ${JSON.stringify(shape)}`);
      throw e;
    }
  };

  walk(shape);
};

export const buildScene = ({
  canvas,
  context,
  width = 512,
  height = 512,
  view,
  withAxes = true,
  renderer,
  preserveDrawingBuffer = false,
  devicePixelRatio = 1,
}) => {
  const { target = [0, 0, 0], position = [40, 40, 40], up = [0, 1, 1] } = view;
  Object3D.DEFAULT_UP.set(...up);

  const camera = new PerspectiveCamera(27, width / height, 1, 1000000);
  camera.position.set(...position);
  camera.up.set(...up);
  camera.lookAt(...target);

  const scene = new Scene();
  scene.add(camera);

  if (withAxes) {
    const axes = new AxesHelper(5);
    axes.layers.set(SKETCH_LAYER);
    scene.add(axes);
  }

  {
    const light = new SpotLight(0xffffff, 10);
    light.target = camera;
    light.decay = 0.2;
    light.position.set(0.1, 0.1, 1);
    light.layers.enable(SKETCH_LAYER);
    light.layers.enable(GEOMETRY_LAYER);
    camera.add(light);
  }

  {
    // Add spot light for shadows.
    const spotLight = new SpotLight(0xffffff, 10);
    spotLight.angle = Math.PI / 6;
    spotLight.penumbra = 1;
    spotLight.decay = 0.2;
    spotLight.distance = 0;
    spotLight.position.set(20, 20, 20);
    spotLight.castShadow = true;
    spotLight.receiveShadow = true;
    spotLight.shadow.camera.near = 1;
    spotLight.shadow.camera.far = 100000;
    spotLight.shadow.focus = 1;
    spotLight.shadow.mapSize.width = 1024 * 2;
    spotLight.shadow.mapSize.height = 1024 * 2;
    spotLight.layers.enable(SKETCH_LAYER);
    spotLight.layers.enable(GEOMETRY_LAYER);
    scene.add(spotLight);
  }

  if (renderer === undefined) {
    renderer = new WebGLRenderer({
      antialias: true,
      logarithmicDepthBuffer: true,
      canvas,
      context,
      preserveDrawingBuffer,
    });
    renderer.autoClear = false;
    renderer.setSize(width, height, /* updateStyle= */ false);
    renderer.setClearColor(0xeeeeee);
    renderer.antiAlias = true;
    renderer.inputGamma = true;
    renderer.outputGamma = true;
    renderer.setPixelRatio(devicePixelRatio);
    renderer.domElement.style =
      'padding-left: 5px; padding-right: 5px; padding-bottom: 5px; position: absolute; z-index: 1';

    renderer.shadowMap.enabled = true;
    renderer.shadowMapType = PCFShadowMap;
  }
  return { camera, canvas: renderer.domElement, renderer, scene };
};

export const staticDisplay = async ({
  view = {},
  assets,
  canvas,
  shape,
  withAxes = false,
  withGrid = false,
  definitions,
  width,
  height,
  doOutlineEdges = true,
} = {}) => {
  if (!canvas) {
    canvas = new OffscreenCanvas(width, height);
  }

  const geometryLayers = new Layers();
  geometryLayers.set(GEOMETRY_LAYER);

  const planLayers = new Layers();
  planLayers.set(SKETCH_LAYER);

  const {
    camera,
    canvas: displayCanvas,
    renderer,
    scene,
  } = buildScene({
    canvas,
    width,
    height,
    view,
    geometryLayers,
    planLayers,
    withAxes,
    preserveDrawingBuffer: true,
  });

  const render = () => {
    renderer.clear();
    camera.layers.set(GEOMETRY_LAYER);
    renderer.render(scene, camera);

    renderer.clearDepth();

    camera.layers.set(SKETCH_LAYER);
    renderer.render(scene, camera);
  };

  const pageSize = [];

  try {
    await buildMeshes({
      assets,
      shape,
      scene,
      definitions,
      pageSize,
      doOutlineEdges,
    });
  } catch (e) {
    console.log(e.stack);
    throw e;
  }

  render();

  return { canvas: displayCanvas, renderer };
};

export default staticDisplay;
