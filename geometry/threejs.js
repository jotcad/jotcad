/* global OffscreenCanvas */

import {
  AxesHelper,
  Box3,
  BufferGeometry,
  Color,
  EdgesGeometry,
  Float32BufferAttribute,
  Frustum,
  GridHelper,
  Group,
  LineBasicMaterial,
  LineSegments,
  Layers,
  Matrix4,
  Mesh,
  MeshBasicMaterial,
  MeshNormalMaterial,
  MeshStandardMaterial,
  Object3D,
  PCFShadowMap,
  Path,
  PerspectiveCamera,
  Plane,
  PlaneGeometry,
  Points,
  PointsMaterial,
  Scene,
  Shape,
  ShapeGeometry,
  SpotLight,
  Vector2,
  Vector3,
  WebGLRenderer,
  WireframeGeometry,
} from '@jotcad/threejs';

export const GEOMETRY_LAYER = 0;
export const SKETCH_LAYER = 1;

const identityMatrix = new Matrix4();

const makeTranslateMatrix = (translateX, translateY, translateZ) => {
  const translationMatrix = new Matrix4();
  translationMatrix.set(
    1, 0, 0, translateX,
    0, 1, 0, translateY,
    0, 0, 1, translateZ,
    0, 0, 0, 1
  );
  return translationMatrix;
};

const makeScaleMatrix = (scaleX, scaleY, scaleZ) => {
  const scaleMatrix = new Matrix4();
  scaleMatrix.set(
    scaleX, 0,      0,      0,
    0,      scaleY, 0,      0,
    0,      0,      scaleZ, 0,
    0,      0,      0,      1
  );
  return scaleMatrix;
};

export const decodeTf = (tf) => {
  if (tf === undefined) {
    return identityMatrix;
  } else if (typeof(tf) === 'string') {
    const pieces = tf.split(' ');
    switch (pieces.shift()) {
      case 'x': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        matrix.rotateX(Math.PI * 2 * tau);
        return Matrix4.makeRotationX(Math.PI * 2 * tau);
      }
      case 'y': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        return Matrix4.makeRotationY(Math.PI * 2 * tau);
      }
      case 'z': {
        pieces.shift();
        const tau = parseFloat(pieces.shift());
        return Matrix4.makeRotationZ(Math.PI * 2 * tau);
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

export const buildMeshes = async ({
  assets,
  shape,
  scene,
  render,
  definitions,
  pageSize = [],
}) => {
  const walk = async (shape) => {
    if (shape.geometry) {
      const matrix = decodeTf(shape.tf);

      // This requires triangulated geometry.
      const vertices = [];
      const segments = [];
      const triangles = [];

      for (const line of assets.text[shape.geometry].split('\n')) {
        const pieces = line.split(' ');
        const code = pieces.shift();
        switch (code) {
          case 'v':
            while (pieces.length >= 3) {
              pieces.shift();
              pieces.shift();
              pieces.shift();
              vertices.push([parseFloat(pieces.shift()), parseFloat(pieces.shift()), parseFloat(pieces.shift())]);
            }
            break;
          case 'p':
            while (pieces.length >= 1) {
              points.push(parseInt(pieces.shift()));
            }
            break;
          case 's':
            while (pieces.length >= 2) {
             const source = parseInt(pieces.shift());
             const target = parseInt(pieces.shift());
             segments.push([source, target]);
            }
            break;
          case 't': {
            const triangle = [];
            while (pieces.length >= 1) {
              triangle.push(parseInt(pieces.shift()));
            }
            triangles.push(triangle);
          }
        }
      }

      if (segments.length > 0) {
        const bufferGeometry = new BufferGeometry();
        const material = new LineBasicMaterial({
          color: new Color('black'),
          linewidth: 2,
        });
        const positions = [];
        for (const [source, target] of segments) {
          positions.push(...vertices[source], ...vertices[target]);
        }
        bufferGeometry.setAttribute(
          'position',
          new Float32BufferAttribute(positions, 3)
        );
        const mesh = new LineSegments(bufferGeometry, material);
        if (matrix) {
          mesh.applyMatrix4(matrix);
        }
        scene.add(mesh);
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
        const material = new MeshNormalMaterial();
        const mesh = new Mesh(bufferGeometry, material);
        if (matrix) {
          mesh.applyMatrix4(matrix);
        }
        scene.add(mesh);
      }
    }

    if (shape.shapes) {
      for (const subShape of shape.shapes) {
        walk(subShape);
      }
    }
  };

  walk(shape);
};

export const buildScene = ({
  canvas,
  context,
  width,
  height,
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

export const staticDisplay = async (
  {
    view = {},
    assets,
    canvas,
    shape,
    withAxes = false,
    withGrid = false,
    definitions,
    width,
    height
  } = {},
) => {
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
    await buildMeshes({ assets, shape, scene, definitions, pageSize });
  } catch (e) {
    console.log(e.stack);
    throw e;
  }

  render();

  return { canvas: displayCanvas, renderer };
};

export default staticDisplay;
