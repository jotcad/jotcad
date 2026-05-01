import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import earcut from 'earcut';
import { ratioToNumber } from './ft.js';

let renderer = null;
let renderRequested = false;
const viewports = new Map(); // id -> {canvas, scene, camera, controls, needsUpdate}

export function requestRender() {
  renderRequested = true;
}

function renderLoop() {
  updateViewports();
  requestAnimationFrame(renderLoop);
}

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

  const lines = text.split('\n');
  let i = 0;

  while (i < lines.length) {
    const line = lines[i].trim();
    if (!line) { i++; continue; }
    
    const pieces = line.split(/\s+/);
    const code = pieces.shift();
    
    if (code === 'V' || code === 'v') {
      const count = parseInt(pieces[0]);
      if (!isNaN(count)) {
        // Header-based: read next 'count' lines
        i++;
        for (let j = 0; j < count && i < lines.length; j++, i++) {
          const vLine = lines[i].trim().split(/\s+/);
          if (vLine.length >= 3) {
            vertices.push([
              ratioToNumber(vLine[0]),
              ratioToNumber(vLine[1]),
              ratioToNumber(vLine[2]),
            ]);
          }
        }
      } else {
        // Legacy: prefix-based single line
        if (pieces.length >= 3) {
          vertices.push([
            ratioToNumber(pieces[0]),
            ratioToNumber(pieces[1]),
            ratioToNumber(pieces[2]),
          ]);
        }
        i++;
      }
    } else if (code === 'F' || code === 'f') {
      const count = parseInt(pieces[0]);
      if (!isNaN(count)) {
        i++;
        for (let j = 0; j < count && i < lines.length; j++, i++) {
          const fLine = lines[i].trim().split(/\s+/);
          if (fLine.length === 0) continue;
          const numLoops = parseInt(fLine.shift() || '1');
          for (let l = 0; l < numLoops; l++) {
             const loopLen = parseInt(fLine.shift() || '0');
             const loop = [];
             for (let k = 0; k < loopLen; k++) {
                const idx = parseInt(fLine.shift() || '-1');
                if (!isNaN(idx) && idx >= 0) loop.push(idx);
             }
             if (loop.length > 0) {
                if (l === 0) faces.push([loop]);
                else faces.at(-1).push(loop);
             }
          }
        }
      } else {
        // Legacy or single face logic could go here
        i++;
      }
    } else if (code === 'P' || code === 'p') {
      const count = parseInt(pieces[0]);
      if (!isNaN(count)) {
        // P <count>\n<v1> <v2>...
        i++;
        const pLine = lines[i].trim().split(/\s+/);
        for (const p of pLine) {
          const idx = parseInt(p);
          if (!isNaN(idx)) points.push(idx);
        }
      } else {
        for (const p of pieces) {
          const idx = parseInt(p);
          if (!isNaN(idx)) points.push(idx);
        }
      }
      i++;
    } else if (code === 'S' || code === 's') {
      const count = parseInt(pieces[0]);
      if (!isNaN(count)) {
        i++;
        for (let j = 0; j < count && i < lines.length; j++, i++) {
          const sLine = lines[i].trim().split(/\s+/);
          if (sLine.length >= 2) {
            segments.push([parseInt(sLine[0]), parseInt(sLine[1])]);
          }
        }
      } else {
        for (let j = 0; j + 1 < pieces.length; j += 2) {
          segments.push([parseInt(pieces[j]), parseInt(pieces[j+1])]);
        }
        i++;
      }
    } else if (code === 'T' || code === 't') {
      const count = parseInt(pieces[0]);
      if (!isNaN(count)) {
        i++;
        for (let j = 0; j < count && i < lines.length; j++, i++) {
          const tLine = lines[i].trim().split(/\s+/);
          if (tLine.length >= 3) {
            triangles.push([parseInt(tLine[0]), parseInt(tLine[1]), parseInt(tLine[2])]);
          }
        }
      } else {
        for (let j = 0; j + 2 < pieces.length; j += 3) {
          triangles.push([parseInt(pieces[j]), parseInt(pieces[j+1]), parseInt(pieces[j+2])]);
        }
        i++;
      }
    } else {
      i++;
    }
  }
  return { vertices, points, triangles, segments, faces };
};


const identity = new THREE.Matrix4();

export const decodeTf = (tf) => {
  if (!tf) return identity;
  const m = new THREE.Matrix4();
  if (Array.isArray(tf)) {
    const flat = tf.map(ratioToNumber).flat();
    m.fromArray(flat);
  } else if (typeof tf === 'string') {
    const flat = ratioToNumber(tf);
    if (Array.isArray(flat)) m.fromArray(flat);
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
    if (s.components) for (const sub of s.components) await walk(sub, worldMat);
  };
  await walk(shape);
};

export function initSharedRenderer() {
  if (renderer) return renderer;
  renderer = new THREE.WebGLRenderer({ 
    antialias: true, 
    alpha: true,
    preserveDrawingBuffer: true,
    powerPreference: "high-performance"
  });
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

  if (typeof document !== 'undefined') {
    document.body.appendChild(renderer.domElement);
  }

  window.addEventListener('resize', () => {
    renderer.setSize(window.innerWidth, window.innerHeight);
    requestRender();
  });

  // Start the global render loop
  requestAnimationFrame(renderLoop);

  return renderer;
}

export function registerViewport(id, canvas, scene, camera, controls) {
  viewports.set(id, { canvas, scene, camera, controls, needsUpdate: true });
  requestRender();
  return () => viewports.delete(id);
}

export function unregisterViewport(id) {
  viewports.delete(id);
  requestRender();
}

export function updateViewports() {
  if (!renderer) return;

  // We only render if:
  // 1. A global render was requested (resize, new data, UI change)
  // 2. ANY viewport's controls are currently animating (damping)
  // 3. ANY viewport was just added/updated
  let shouldRender = renderRequested;
  
  viewports.forEach(vp => {
    if (vp.controls && vp.controls.update()) {
      shouldRender = true;
    }
    if (vp.needsUpdate) {
      shouldRender = true;
      vp.needsUpdate = false;
    }
  });

  if (!shouldRender) return;
  renderRequested = false;

  renderer.setClearColor(0x000000, 0);
  renderer.setScissorTest(false);
  renderer.clear();
  renderer.setScissorTest(true);

  viewports.forEach((vp) => {
    if (!vp.canvas || typeof vp.canvas.getBoundingClientRect !== 'function') return;
    const rect = vp.canvas.getBoundingClientRect();
    if (rect.width === 0 || rect.height === 0 || rect.bottom < 0 || rect.top > window.innerHeight) return;

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
      // Support both Selector objects and direct CID strings
      const data = await this.vfs.readData(id);
      if (data) {
        const text = typeof data === 'string' ? data : new TextDecoder().decode(data);
        this.cache.set(cacheKey, text);
        return text;
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
      const id = normalizeId(s.geometry);
      if (!assets.has(id)) {
        try {
          const data = await vfs.readData(s.geometry);
          if (data) {
            const text = typeof data === 'string' ? data : new TextDecoder().decode(data);
            assets.set(id, text);
          }
        } catch (e) { console.warn('[packZFS] Failed to fetch asset:', id, e); }
      }
    }
    if (s.components && Array.isArray(s.components)) {
      for (const sub of s.components) await walk(sub);
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
