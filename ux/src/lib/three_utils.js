import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import earcut from 'earcut';
import { ratioToNumber } from './ft.js';

// The "Hitchhiker" - One renderer to rule them all
let sharedRenderer = null;
let activeViewport = null; // { container, scene, camera, controls, id }

// Dummy exports for backward compatibility
export function initSharedRenderer() { return getSharedRenderer(); }
export function registerViewport() { return () => {}; }
export function unregisterViewport() { }
export function requestRender() { }

export function getSharedRenderer() {
  if (sharedRenderer) return sharedRenderer;
  sharedRenderer = new THREE.WebGLRenderer({ 
    antialias: true, 
    alpha: true,
    preserveDrawingBuffer: true,
    powerPreference: "high-performance"
  });
  sharedRenderer.setPixelRatio(window.devicePixelRatio);
  sharedRenderer.setClearColor(0x00ffff, 1); // Default Active: Cyan
  sharedRenderer.autoClear = true;

  const loop = () => {
    if (activeViewport && sharedRenderer) {
      const { scene, camera, controls } = activeViewport;
      if (controls) controls.update();
      sharedRenderer.setClearColor(0x00ffff, 1); // Active: Cyan
      sharedRenderer.render(scene, camera);
    }
    requestAnimationFrame(loop);
  };
  requestAnimationFrame(loop);

  return sharedRenderer;
}

export function createLabel(text, color = 'white', size = 12) {
  const canvas = document.createElement('canvas');
  const ctx = canvas.getContext('2d');
  canvas.width = 64;
  canvas.height = 32;
  ctx.font = `Bold ${size}px Monospace`;
  ctx.fillStyle = color;
  ctx.textAlign = 'center';
  ctx.textBaseline = 'middle';
  ctx.fillText(text, 32, 16);

  const texture = new THREE.CanvasTexture(canvas);
  const material = new THREE.SpriteMaterial({ map: texture, transparent: true });
  const sprite = new THREE.Sprite(material);
  sprite.scale.set(10, 5, 1);
  return sprite;
}

export function activateViewport(id, container, scene, camera, controls) {
  const renderer = getSharedRenderer();
  if (activeViewport && activeViewport.id === id && renderer.domElement.parentElement === container) return;

  console.log(`[three_utils] Activating Viewport: ${id}`);

  if (renderer.domElement.parentElement !== container) {
    container.appendChild(renderer.domElement);
  }

  const rect = container.getBoundingClientRect();
  const width = rect.width || container.clientWidth || 300;
  const height = rect.height || container.clientHeight || 200;
  
  renderer.setSize(width, height);
  renderer.domElement.style.width = '100%';
  renderer.domElement.style.height = '100%';
  renderer.domElement.style.display = 'block';

  camera.aspect = width / height;
  camera.updateProjectionMatrix();

  activeViewport = { id, container, scene, camera, controls };
}

/**
 * Captures a Teal snapshot of a specific scene/camera.
 */
export function captureSnapshot(scene, camera) {
  const renderer = getSharedRenderer();
  if (!scene || !camera) return null;

  // Temporarily switch to Teal for the snapshot
  renderer.setClearColor(0x008080, 1);
  renderer.render(scene, camera);
  
  const data = renderer.domElement.toDataURL('image/png');
  
  // Restore Cyan if we have an active viewport
  renderer.setClearColor(0x00ffff, 1);
  
  return data;
}

// Geometry Decoding Logic
export const decodeGeometry = (text) => {
  const vertices = [], points = [], triangles = [], segments = [], faces = [];
  if (!text) return { vertices, points, triangles, segments, faces };
  const lines = text.split('\n');
  let i = 0;
  while (i < lines.length) {
    const line = lines[i].trim();
    if (!line) { i++; continue; }
    const pieces = line.split(/\s+/);
    const code = pieces.shift();
    if (code === 'V') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const vLine = lines[i].trim().split(/\s+/);
        if (vLine.length >= 3) vertices.push([ratioToNumber(vLine[0]), ratioToNumber(vLine[1]), ratioToNumber(vLine[2])]);
      }
    } else if (code === 'F') {
      const count = parseInt(pieces[0]); i++;
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
    } else if (code === 'P') {
      const count = parseInt(pieces[0]); i++;
      if (count > 0 && i < lines.length) {
        const pLine = lines[i].trim().split(/\s+/);
        for (let idxStr of pLine) { const idx = parseInt(idxStr); if (!isNaN(idx)) points.push(idx); }
        i++;
      }
    } else if (code === 'S') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const sLine = lines[i].trim().split(/\s+/);
        if (sLine.length >= 2) segments.push([parseInt(sLine[0]), parseInt(sLine[1])]);
      }
    } else if (code === 'T') {
      const count = parseInt(pieces[0]); i++;
      for (let j = 0; j < count && i < lines.length; j++, i++) {
        const tLine = lines[i].trim().split(/\s+/);
        if (tLine.length >= 3) triangles.push([parseInt(tLine[0]), parseInt(tLine[1]), parseInt(tLine[2])]);
      }
    } else i++;
  }
  return { vertices, points, triangles, segments, faces };
};

const identity = new THREE.Matrix4();
export const decodeTf = (tf) => {
  if (!tf) return identity;
  const m = new THREE.Matrix4();
  let flat = [];

  if (Array.isArray(tf)) {
    flat = tf.map(ratioToNumber).flat();
  } else if (typeof tf === 'string') {
    flat = ratioToNumber(tf);
    if (!Array.isArray(flat)) flat = [flat];
  }

  // Handle JOT 3x4 Row-Major (12 elements) -> THREE 4x4 Column-Major (16 elements)
  if (flat.length === 12) {
    m.set(
      flat[0], flat[1], flat[2],  flat[3],
      flat[4], flat[5], flat[6],  flat[7],
      flat[8], flat[9], flat[10], flat[11],
      0,       0,       0,        1
    );
  } else if (flat.length === 16) {
    m.fromArray(flat);
  }

  return m;
};

const toColor = (tags) => {
  if (!tags) return new THREE.Color(0x808080);
  const colorVal = tags.color || tags.tags?.color;
  if (colorVal) return new THREE.Color(colorVal);
  for (const key of Object.keys(tags)) if (key.startsWith('color:')) return new THREE.Color(key.substring(6));
  return new THREE.Color(0x808080);
};

export const buildMeshes = async ({ assets, shape, scene, edgeThreshold = 15 }) => {
  if (!scene.getObjectByName('ambient_light')) {
    const ambient = new THREE.AmbientLight(0xffffff, 0.6); ambient.name = 'ambient_light'; scene.add(ambient);
    const directional = new THREE.DirectionalLight(0xffffff, 0.6); directional.position.set(10, 20, 10); directional.name = 'directional_light'; scene.add(directional);
  }
  const walk = async (s) => {
    const worldMat = decodeTf(s.tf);
    const shapeColor = toColor(s.tags);
    if (s.geometry) {
      const text = await assets.getText(s.geometry);
      if (text) {
        const { vertices, triangles, segments } = decodeGeometry(text);
        if (vertices.length === 0) return;

        // Strict Triangle-Only Path
        if (triangles.length > 0) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.Float32BufferAttribute(vertices.flat(), 3));
            g.setIndex(triangles.flat());
            g.computeVertexNormals();

            const mesh = new THREE.Mesh(g, new THREE.MeshStandardMaterial({ 
                color: shapeColor, side: THREE.DoubleSide, roughness: 0.5, metalness: 0.1 
            }));
            mesh.userData.isJot = true;
            mesh.applyMatrix4(worldMat); scene.add(mesh);
            
            const edgesGeo = new THREE.EdgesGeometry(g, edgeThreshold);
            const edgesLine = new THREE.LineSegments(edgesGeo, new THREE.LineBasicMaterial({ color: 0xffffff, transparent: true, opacity: 0.5 }));
            edgesLine.userData.isJot = true;
            edgesLine.applyMatrix4(worldMat); scene.add(edgesLine);
        }

        if (segments.length > 0) {
            const pos = [];
            for (const seg of segments) {
                const v0 = vertices[seg[0]];
                const v1 = vertices[seg[1]];
                if (v0 && v1) {
                    pos.push(...v0, ...v1);
                }
            }
            if (pos.length > 0) {
                const g = new THREE.BufferGeometry();
                g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
                const line = new THREE.LineSegments(g, new THREE.LineBasicMaterial({ 
                    color: shapeColor, 
                    linewidth: 2, // Slightly increased visibility
                    transparent: false 
                }));
                line.userData.isJot = true;
                line.applyMatrix4(worldMat); 
                scene.add(line);
            }
        }
      }
    }
    if (s.components) for (const sub of s.components) await walk(sub);
  };
  await walk(shape);
};

const normalizeId = (id) => {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => { acc[key] = params[key]; return acc; }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
};

class JOTAssets {
  constructor(vfs) { this.vfs = vfs; this.cache = new Map(); this.main = null; }
  async parse(text) {
    let offset = 0; if (text[0] === '\n') offset++;
    while (offset < text.length) {
      if (text[offset] !== '=') break;
      offset++;
      const nl = text.indexOf('\n', offset), h = text.substring(offset, nl);
      offset = nl + 1;
      const sp = h.indexOf(' '), len = parseInt(h.substring(0, sp), 10), name = h.substring(sp + 1);
      const content = text.substring(offset, offset + len);
      offset += len; if (text[offset] === '\n') offset++;
      if (name.startsWith('files/')) this.main = JSON.parse(content);
      else if (name.startsWith('assets/text/')) this.cache.set(name.substring(12), content);
    }
    return this.main;
  }
  async getText(id) {
    const cacheKey = normalizeId(id);
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);
    if (!this.vfs) return null;
    try {
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

export async function renderJotToScene(vfs, data, scene, edgeThreshold = 15) {
  const assets = new JOTAssets(vfs);
  let shape;
  let textData = data;
  if (data instanceof Uint8Array) textData = new TextDecoder().decode(data);
  
  if (textData) {
    console.log('[three_utils] Received Geometry Data:', textData);
  }

  if (typeof textData === 'string') {
    const t = textData.trim();
    if (t.startsWith('=') || t.startsWith('\n=')) shape = await assets.parse(textData);
    else if (t.startsWith('{')) shape = JSON.parse(t);
    else if (t.includes('v ')) {
      const id = 'raw-' + Math.random().toString(36).slice(2);
      assets.cache.set(id, textData);
      shape = { geometry: id };
    }
  } else if (typeof textData === 'object') shape = textData;
  if (shape) await buildMeshes({ assets, shape, scene, edgeThreshold });
  return shape;
}

export async function packZFS(vfs, shape) {
  const assets = new Map();
  const walk = async (s) => {
    if (!s || typeof s !== 'object') return;
    if (s.geometry) {
      const id = normalizeId(s.geometry);
      if (!assets.has(id)) {
        try {
          const data = await vfs.readData(s.geometry);
          if (data) assets.set(id, typeof data === 'string' ? data : new TextDecoder().decode(data));
        } catch (e) { console.warn('[packZFS] Failed to fetch asset:', id, e); }
      }
    }
    if (s.components && Array.isArray(s.components)) for (const sub of s.components) await walk(sub);
  };
  await walk(shape);
  let zfs = '';
  const mainJson = JSON.stringify(shape);
  zfs += `=${mainJson.length} files/main.json\n${mainJson}\n`;
  for (const [id, text] of assets) zfs += `=${text.length} assets/text/${id}\n${text}\n`;
  return zfs;
}

export function createScene() { const scene = new THREE.Scene(); scene.add(new THREE.AxesHelper(10)); return scene; }
const thumbCache = new Map();
export async function captureThumbnail(vfs, data, width = 256, height = 256) {
  const cacheKey = typeof data === 'string' ? data.length + data.slice(0, 100) : JSON.stringify(data).length;
  if (thumbCache.has(cacheKey)) return thumbCache.get(cacheKey);
  const renderer = getSharedRenderer();
  const scene = new THREE.Scene();
  try { await renderJotToScene(vfs, data, scene); } catch (e) { return null; }
  if (scene.children.length === 0) return null;
  const b = new THREE.Box3().setFromObject(scene), c = b.getCenter(new THREE.Vector3()), s = b.getSize(new THREE.Vector3()), d = Math.max(s.x, s.y, s.z) || 1;
  const cam = new THREE.PerspectiveCamera(45, width / height, 0.1, 10000);
  cam.position.set(c.x, c.y, c.z + d * 2); cam.up.set(0, 1, 0); cam.lookAt(c);
  scene.add(new THREE.AmbientLight(0xffffff, 0.7));
  const dl = new THREE.DirectionalLight(0xffffff, 0.8); dl.position.set(d, d * 2, d); scene.add(dl);
  const rt = new THREE.WebGLRenderTarget(width, height);
  renderer.setRenderTarget(rt); renderer.render(scene, cam);
  const buf = new Uint8Array(width * height * 4);
  renderer.readRenderTargetPixels(rt, 0, 0, width, height, buf);
  renderer.setRenderTarget(null);
  const canvas = document.createElement('canvas'); canvas.width = width; canvas.height = height;
  const ctx = canvas.getContext('2d'), img = ctx.createImageData(width, height);
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const src = (y * width + x) * 4, dst = ((height - y - 1) * width + x) * 4;
    for (let i = 0; i < 4; i++) img.data[dst + i] = buf[src + i];
  }
  ctx.putImageData(img, 0, 0);
  const dataUrl = canvas.toDataURL('image/png'); thumbCache.set(cacheKey, dataUrl); return dataUrl;
}
