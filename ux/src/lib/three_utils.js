import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import earcut from 'earcut';

let renderer = null;
const viewports = new Map(); // id -> {canvas, scene, camera}

/**
 * Decodes the custom JotCAD Geometry text format.
 * Format:
 * v x1 y1 z1 x2 y2 z2 (Vertices: low precision then high precision)
 * p v1 v2... (Points)
 * s v1 v2... (Segments/Edges)
 * t v1 v2 v3 (Triangles)
 * f v1 v2 v3... (Face loop)
 * h v1 v2 v3... (Hole loop for previous face)
 * Standardizes on 0-based indexing for internal Three.js use.
 */
export const decodeGeometry = (text) => {
  const vertices = [],
    points = [],
    triangles = [],
    segments = [],
    faces = [];
  if (!text) return { vertices, points, triangles, segments, faces };

  for (const line of text.split('\n')) {
    const pieces = line.trim().split(/\s+/);
    if (pieces.length === 0) continue;
    const code = pieces.shift();
    if (!code) continue;

    switch (code) {
      case 'v':
        // JOT format: v <inexact_x> <inexact_y> <inexact_z> [<exact_x> <exact_y> <exact_z>]
        // JS always uses the first triplet (inexact) for visualization.
        if (pieces.length >= 3) {
          vertices.push([
            parseFloat(pieces[0]),
            parseFloat(pieces[1]),
            parseFloat(pieces[2]),
          ]);
        }
        break;
      case 'p':
        for (const p of pieces) {
          const idx = parseInt(p);
          if (!isNaN(idx)) points.push(idx);
        }
        break;
      case 's':
        for (let i = 0; i + 1 < pieces.length; i += 2) {
          segments.push([
            parseInt(pieces[i]),
            parseInt(pieces[i+1])
          ]);
        }
        break;
      case 't':
        for (let i = 0; i + 2 < pieces.length; i += 3) {
          triangles.push([
            parseInt(pieces[i]),
            parseInt(pieces[i+1]),
            parseInt(pieces[i+2]),
          ]);
        }
        break;
      case 'f': {
        const loop = [];
        for (const p of pieces) {
          const idx = parseInt(p);
          if (!isNaN(idx)) loop.push(idx);
        }
        if (loop.length > 0) faces.push([loop]);
        break;
      }
      case 'h': {
        const loop = [];
        for (const p of pieces) {
          const idx = parseInt(p);
          if (!isNaN(idx)) loop.push(idx);
        }
        if (faces.length > 0 && loop.length > 0) faces.at(-1).push(loop);
        break;
      }
    }
  }
  return { vertices, points, triangles, segments, faces };
};

const identity = new THREE.Matrix4();
export const decodeTf = (tf) => {
  if (!tf) return identity;
  const m = new THREE.Matrix4();
  if (Array.isArray(tf)) {
    m.fromArray(tf);
  } else if (typeof tf === 'object') {
    // Handle specific tf object if needed
  }
  return m;
};

const toColor = (tags) => {
  if (!tags) return new THREE.Color(0x808080);
  const colorVal = tags.color || tags.tags?.color; // Handle both flat and nested tags
  if (colorVal) {
    return new THREE.Color(colorVal);
  }
  // Handle legacy color:red style tags
  for (const key of Object.keys(tags)) {
    if (key.startsWith('color:')) return new THREE.Color(key.substring(6));
  }
  return new THREE.Color(0x808080);
};

export const buildMeshes = async ({ assets, shape, scene }) => {
  // Add lights if not already present in the scene for Lambert/Phong materials
  if (!scene.getObjectByName('ambient_light')) {
    const ambient = new THREE.AmbientLight(0xffffff, 0.6);
    ambient.name = 'ambient_light';
    scene.add(ambient);
    const directional = new THREE.DirectionalLight(0xffffff, 0.6);
    directional.position.set(10, 20, 10);
    directional.name = 'directional_light';
    scene.add(directional);
  }

  const walk = async (s, parentMat = identity) => {
    const worldMat = parentMat.clone().multiply(decodeTf(s.tf));
    const shapeColor = toColor(s.tags);

    if (s.geometry) {
      const text = await assets.getText(s.geometry);
      if (text) {
        const { vertices, triangles, segments, faces } = decodeGeometry(text);
        
        // 1. Triangles
        if (triangles.length > 0) {
          const pos = [], norm = [];
          for (const tri of triangles) {
            const v0 = vertices[tri[0]], v1 = vertices[tri[1]], v2 = vertices[tri[2]];
            if (!v0 || !v1 || !v2) continue;
            const vec0 = new THREE.Vector3(...v0), vec1 = new THREE.Vector3(...v1), vec2 = new THREE.Vector3(...v2);
            const n = new THREE.Vector3().crossVectors(
              new THREE.Vector3().subVectors(vec2, vec1),
              new THREE.Vector3().subVectors(vec0, vec1)
            ).normalize();
            pos.push(...v0, ...v1, ...v2);
            for (let i = 0; i < 3; i++) norm.push(n.x, n.y, n.z);
          }
          if (pos.length > 0) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
            g.setAttribute('normal', new THREE.Float32BufferAttribute(norm, 3));
            const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ 
                color: shapeColor,
                side: THREE.DoubleSide 
            }));
            mesh.applyMatrix4(worldMat);
            scene.add(mesh);
          }
        }

        // 2. Segments
        if (segments.length > 0) {
          const pos = [];
          for (const seg of segments) {
            const v0 = vertices[seg[0]], v1 = vertices[seg[1]];
            if (v0 && v1) pos.push(...v0, ...v1);
          }
          if (pos.length > 0) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
            const line = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ color: shapeColor }));
            line.applyMatrix4(worldMat);
            scene.add(line);
          }
        }

        // 3. Faces
        for (const [face, ...holes] of faces) {
          const outer = face.map((i) => vertices[i]).filter(Boolean);
          if (outer.length < 3) continue;
          const hls = holes.map((h) => h.map((i) => vertices[i]).filter(Boolean));

          const n = new THREE.Vector3(0, 0, 0);
          for (let i = 0; i < outer.length; i++) {
            const p1 = outer[i];
            const p2 = outer[(i + 1) % outer.length];
            n.x += (p1[1] - p2[1]) * (p1[2] + p2[2]);
            n.y += (p1[2] - p2[2]) * (p1[0] + p2[0]);
            n.z += (p1[0] - p2[0]) * (p1[1] + p2[1]);
          }
          n.normalize();
          
          if (n.lengthSq() < 0.0001) n.set(0, 0, 1); // Fallback for failed normal
          
          let u = 0, v = 1;
          const nx = Math.abs(n.x), ny = Math.abs(n.y), nz = Math.abs(n.z);
          if (nx >= ny && nx >= nz) { u = 1; v = 2; }
          else if (ny >= nx && ny >= nz) { u = 0; v = 2; }
          else { u = 0; v = 1; }

          const flat = [], hIdx = [];
          for (const p of outer) flat.push(p[u], p[v]);
          for (const h of hls) {
            hIdx.push(flat.length / 2);
            for (const p of h) flat.push(p[u], p[v]);
          }

          const indices = earcut(flat, hIdx);
          const allVertices = [...outer, ...hls.flat()];
          const posArr = allVertices.flatMap((p) => [p[0], p[1], p[2]]);
          const normArr = allVertices.flatMap(() => [n.x, n.y, n.z]);
            
          const g = new THREE.BufferGeometry();
          g.setAttribute('position', new THREE.Float32BufferAttribute(posArr, 3));
          g.setAttribute('normal', new THREE.Float32BufferAttribute(normArr, 3));
          g.setIndex(indices);
          
          const mesh = new THREE.Mesh(g, new THREE.MeshLambertMaterial({ 
              color: shapeColor,
              side: THREE.DoubleSide 
          }));
          mesh.applyMatrix4(worldMat);
          scene.add(mesh);
        }
      }
    }
    if (s.shapes) for (const sub of s.shapes) await walk(sub, worldMat);
  };
  await walk(shape);
};

export function initSharedRenderer() {
  if (renderer) return renderer;
  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
  renderer.setPixelRatio(window.devicePixelRatio);
  renderer.setSize(window.innerWidth, window.innerHeight);
  renderer.setClearColor(0x000000, 0);

  renderer.domElement.style.position = 'fixed';
  renderer.domElement.style.top = '0';
  renderer.domElement.style.left = '0';
  renderer.domElement.style.width = '100vw';
  renderer.domElement.style.height = '100vh';
  renderer.domElement.style.pointerEvents = 'none';
  renderer.domElement.style.zIndex = '1000';
  renderer.domElement.style.margin = '0';
  renderer.domElement.style.padding = '0';

  window.addEventListener('resize', () => {
    renderer.setSize(window.innerWidth, window.innerHeight);
  });

  return renderer;
}

export function registerViewport(id, canvas, scene, camera) {
  viewports.set(id, { canvas, scene, camera });
  return () => viewports.delete(id);
}

export function unregisterViewport(id) {
  viewports.delete(id);
}

export function updateViewports() {
  if (!renderer) initSharedRenderer();
  if (!renderer) return;

  renderer.setClearColor(0x000000, 0);
  renderer.setScissorTest(false);
  renderer.clear();
  renderer.setScissorTest(true);

  viewports.forEach((vp) => {
    if (!vp.canvas || typeof vp.canvas.getBoundingClientRect !== 'function') return;
    const rect = vp.canvas.getBoundingClientRect();
    if (rect.width === 0 || rect.height === 0 || rect.bottom < 0 || rect.top > window.innerHeight) return;

    // Use logical pixels for setViewport/setScissor when setPixelRatio is active
    const width = rect.width;
    const height = rect.height;
    const left = rect.left;
    const bottom = window.innerHeight - rect.bottom;

    renderer.setViewport(left, bottom, width, height);
    renderer.setScissor(left, bottom, width, height);
    
    if (vp.scene.background) {
      renderer.setClearColor(vp.scene.background, 1.0);
      renderer.clear();
    }

    vp.camera.aspect = width / height;
    vp.camera.updateProjectionMatrix();
    renderer.render(vp.scene, vp.camera);
  });
}

const normalizeId = (id) => {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    // Sort keys for deterministic JSON
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => {
      acc[key] = params[key];
      return acc;
    }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
};

class JOTAssets {
  constructor(vfs) {
    this.vfs = vfs;
    this.cache = new Map();
    this.main = null;
  }
  async parse(text) {
    let offset = 0;
    if (text[0] === '\n') offset++;
    while (offset < text.length) {
      if (text[offset] !== '=') break;
      offset++;
      const nl = text.indexOf('\n', offset), h = text.substring(offset, nl);
      offset = nl + 1;
      const sp = h.indexOf(' '), len = parseInt(h.substring(0, sp), 10), name = h.substring(sp + 1);
      const content = text.substring(offset, offset + len);
      offset += len;
      if (text[offset] === '\n') offset++;
      if (name.startsWith('files/')) this.main = JSON.parse(content);
      else if (name.startsWith('assets/text/')) {
        const id = name.substring('assets/text/'.length);
        // We don't normalize here because the ZFS should already be in normalized format
        // OR we can normalize the name if it's a URL-style string
        this.cache.set(id, content);
      }
    }
    return this.main;
  }
  async getText(id) {
    const cacheKey = normalizeId(id);
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);
    
    // Attempt fallback to URL-style if JSON-style fails (for legacy or manual ZFS)
    if (typeof id === 'object' && id.path && id.parameters) {
        const urlKey = id.path + '?' + new URLSearchParams(id.parameters).toString();
        if (this.cache.has(urlKey)) return this.cache.get(urlKey);
    }

    if (!this.vfs) return null;
    try {
      if (typeof id === 'object' && id.path) {
        const data = await this.vfs.readData(id.path, id.parameters);
        if (data) {
          const text = typeof data === 'string' ? data : new TextDecoder().decode(data);
          this.cache.set(cacheKey, text);
          return text;
        }
      }
    } catch (e) { console.warn('[JOTAssets] Resolution failed:', id, e); }
    return null;
  }
}

export async function packZFS(vfs, shape) {
  const assets = new Map(); // id -> text
  const walk = async (s) => {
    if (!s || typeof s !== 'object') return;

    if (s.geometry) {
      const g = s.geometry;
      const path = g.path || (typeof g === 'string' ? g : null);
      const params = g.parameters || {};
      if (path) {
        const id = normalizeId({ path, parameters: params });
        if (!assets.has(id)) {
          try {
            const text = await vfs.readText(path, params);
            if (text) assets.set(id, text);
          } catch (e) { console.warn('[packZFS] Failed to fetch asset:', id, e); }
        }
      }
    }
    if (s.shapes && Array.isArray(s.shapes)) {
      for (const sub of s.shapes) await walk(sub);
    }
  };
  await walk(shape);

  let zfs = '';
  const mainJson = JSON.stringify(shape);
  zfs += `=${mainJson.length} files/main.json\n${mainJson}\n`;
  for (const [id, text] of assets) {
    zfs += `=${text.length} assets/text/${id}\n${text}\n`;
  }
  return zfs;
}

export async function renderJotToScene(vfs, data, scene) {
  const assets = new JOTAssets(vfs);
  let shape;
  let textData = data;
  if (data instanceof Uint8Array) textData = new TextDecoder().decode(data);

  if (typeof textData === 'string') {
    const t = textData.trim();
    if (t.startsWith('=') || t.startsWith('\n=')) {
      // Log the structural ZFS block
      console.log('[renderJotToScene] Decoding Unified ZFS:\n' + textData);
      shape = await assets.parse(textData);
    } else if (t.startsWith('{')) {
      shape = JSON.parse(t);
    } else if (t.includes('v ')) {
      const id = 'raw-' + Math.random().toString(36).slice(2);
      assets.cache.set(id, textData);
      shape = { geometry: id };
    }
  } else if (typeof textData === 'object') shape = textData;

  if (shape) await buildMeshes({ assets, shape, scene });
  return shape;
}

export function createScene() {
  const scene = new THREE.Scene();
  scene.add(new THREE.AxesHelper(10)); // X=Red, Y=Green, Z=Blue
  return scene;
}

const thumbCache = new Map(); // CID -> dataURL

export async function captureThumbnail(vfs, data, width = 256, height = 256) {
  // Use a simple hash of the data as a cache key if CID isn't available
  const cacheKey =
    typeof data === 'string'
      ? data.length + data.slice(0, 100)
      : JSON.stringify(data).length;
  if (thumbCache.has(cacheKey)) return thumbCache.get(cacheKey);

  if (!renderer) initSharedRenderer();
  const scene = new THREE.Scene();

  try {
    await renderJotToScene(vfs, data, scene);
  } catch (e) {
    console.warn('[Thumbnail] Render failed:', e);
    return null;
  }

  // Ensure there's actually something in the scene
  if (scene.children.length === 0) return null;

  const b = new THREE.Box3().setFromObject(scene);
  const c = b.getCenter(new THREE.Vector3());
  const s = b.getSize(new THREE.Vector3());
  const d = Math.max(s.x, s.y, s.z) || 1;

  const cam = new THREE.PerspectiveCamera(45, width / height, 0.1, 10000);
  // Match the interactive view: look from 0,0,1 relative to the center
  cam.position.set(c.x, c.y, c.z + d * 2);
  cam.up.set(0, 1, 0);
  cam.lookAt(c);

  // Add lights to the temporary scene
  scene.add(new THREE.AmbientLight(0xffffff, 0.7));
  const dl = new THREE.DirectionalLight(0xffffff, 0.8);
  dl.position.set(d, d * 2, d);
  scene.add(dl);

  const rt = new THREE.WebGLRenderTarget(width, height);
  renderer.setRenderTarget(rt);
  renderer.render(scene, cam);

  const buf = new Uint8Array(width * height * 4);
  renderer.readRenderTargetPixels(rt, 0, 0, width, height, buf);
  renderer.setRenderTarget(null);

  const canvas = document.createElement('canvas');
  canvas.width = width;
  canvas.height = height;
  const ctx = canvas.getContext('2d'),
    img = ctx.createImageData(width, height);
  for (let y = 0; y < height; y++)
    for (let x = 0; x < width; x++) {
      const src = (y * width + x) * 4,
        dst = ((height - y - 1) * width + x) * 4;
      for (let i = 0; i < 4; i++) img.data[dst + i] = buf[src + i];
    }
  ctx.putImageData(img, 0, 0);
  const dataUrl = canvas.toDataURL('image/png');
  thumbCache.set(cacheKey, dataUrl);
  return dataUrl;
}
