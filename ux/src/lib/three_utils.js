import * as THREE from 'three';
import earcut from 'earcut';

let renderer;
const viewports = new Map();

export function initSharedRenderer() {
    if (renderer) return renderer;
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setClearColor(0x000000, 0);
    return renderer;
}

export function createScene() {
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 1000);
    camera.position.set(5, 5, 5);
    camera.lookAt(0, 0, 0);
    scene.add(new THREE.AmbientLight(0xffffff, 0.5));
    const dl = new THREE.DirectionalLight(0xffffff, 0.8);
    dl.position.set(10, 10, 10);
    scene.add(dl);
    return { scene, camera };
}

export function updateViewports() {
    if (!renderer) return;
    renderer.setClearColor(0x000000, 0);
    renderer.setScissorTest(false);
    renderer.clear();
    renderer.setScissorTest(true);
    viewports.forEach((vp) => {
        const rect = vp.canvas.getBoundingClientRect();
        if (rect.bottom < 0 || rect.top > renderer.domElement.clientHeight ||
            rect.right < 0 || rect.left > renderer.domElement.clientWidth) return;
        const width = rect.right - rect.left;
        const height = rect.bottom - rect.top;
        const left = rect.left;
        const bottom = renderer.domElement.clientHeight - rect.bottom;
        renderer.setViewport(left, bottom, width, height);
        renderer.setScissor(left, bottom, width, height);
        vp.camera.aspect = width / height;
        vp.camera.updateProjectionMatrix();
        renderer.render(vp.scene, vp.camera);
    });
}

/**
 * Decodes Inexact Geometry Text (v, f, t, h).
 * Standardizes on 0-based indexing for internal Three.js use.
 */
export const decodeGeometry = (text) => {
  const vertices = [], triangles = [], faces = [];
  for (const line of text.split('\n')) {
    const pieces = line.trim().split(' ');
    const code = pieces.shift();
    switch (code) {
      case 'v':
        while (pieces.length >= 3) {
          if (pieces.length >= 6) pieces.splice(0, 3); // Skip precise coords
          vertices.push([parseFloat(pieces.shift()), parseFloat(pieces.shift()), parseFloat(pieces.shift())]);
        }
        break;
      case 't':
        while (pieces.length >= 3) {
          // Convert 1-based to 0-based
          triangles.push([parseInt(pieces.shift()) - 1, parseInt(pieces.shift()) - 1, parseInt(pieces.shift()) - 1]);
        }
        break;
      case 'f': {
        const face = [];
        // Convert 1-based to 0-based
        while (pieces.length >= 1) face.push(parseInt(pieces.shift()) - 1);
        if (face.length > 0) faces.push([face]);
        break;
      }
      case 'h': {
        const hole = [];
        // Convert 1-based to 0-based
        while (pieces.length >= 1) hole.push(parseInt(pieces.shift()) - 1);
        if (faces.length > 0 && hole.length > 0) faces.at(-1).push(hole);
        break;
      }
    }
  }
  return { vertices, triangles, faces };
};

const identity = new THREE.Matrix4();
export const decodeTf = (tf) => {
  if (!tf) return identity;
  if (typeof tf === 'string') {
    const pieces = tf.split(' '), type = pieces.shift(), m = new THREE.Matrix4();
    switch (type) {
      case 'x': case 'y': case 'z':
        pieces.shift();
        const a = Math.PI * 2 * parseFloat(pieces.shift());
        if (type === 'x') m.makeRotationX(a); else if (type === 'y') m.makeRotationY(a); else m.makeRotationZ(a);
        return m;
      case 's': pieces.splice(0, 3); return m.makeScale(parseFloat(pieces.shift()), parseFloat(pieces.shift()), parseFloat(pieces.shift()));
      case 't': pieces.splice(0, 3); return m.makeTranslation(parseFloat(pieces.shift()), parseFloat(pieces.shift()), parseFloat(pieces.shift()));
      case 'm': pieces.splice(0, 13); const v = pieces.map(parseFloat);
        m.set(v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7], v[8], v[9], v[10], v[11], 0, 0, 0, v[12]);
        return m;
      default: return identity;
    }
  } else if (Array.isArray(tf)) {
    const b = decodeTf(tf[1]);
    return tf[0] === 'i' ? b.clone().invert() : decodeTf(tf[0]).clone().multiply(b);
  }
  return identity;
};

export const buildMeshes = async ({ assets, shape, scene }) => {
  const walk = async (s, parentMat = identity) => {
    const worldMat = parentMat.clone().multiply(decodeTf(s.tf));
    if (s.geometry) {
      const text = await assets.getText(s.geometry);
      if (text) {
        const { vertices, triangles, faces } = decodeGeometry(text);
        const material = new THREE.MeshNormalMaterial({ side: THREE.DoubleSide });
        
        if (triangles.length > 0) {
          const pos = [], norm = [];
          for (const tri of triangles) {
            const v0Arr = vertices[tri[0]], v1Arr = vertices[tri[1]], v2Arr = vertices[tri[2]];
            if (!v0Arr || !v1Arr || !v2Arr) continue;

            const v0 = new THREE.Vector3(...v0Arr), v1 = new THREE.Vector3(...v1Arr), v2 = new THREE.Vector3(...v2Arr);
            const n = new THREE.Vector3().crossVectors(new THREE.Vector3().subVectors(v2, v1), new THREE.Vector3().subVectors(v0, v1)).normalize();
            pos.push(v0.x, v0.y, v0.z, v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);
            for(let i=0; i<3; i++) norm.push(n.x, n.y, n.z);
          }
          if (pos.length > 0) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
            g.setAttribute('normal', new THREE.Float32BufferAttribute(norm, 3));
            const mesh = new THREE.Mesh(g, material); mesh.applyMatrix4(worldMat); scene.add(mesh);
          }
        }

        for (const [face, ...holes] of faces) {
          const outer = face.map(i => vertices[i]).filter(v => v !== undefined);
          if (outer.length < 3) continue;

          const hls = holes.map(h => h.map(i => vertices[i]).filter(v => v !== undefined));
          
          const n = new THREE.Vector3(); 
          n.crossVectors(
            new THREE.Vector3().subVectors(new THREE.Vector3(...outer[2]), new THREE.Vector3(...outer[1])), 
            new THREE.Vector3().subVectors(new THREE.Vector3(...outer[0]), new THREE.Vector3(...outer[1]))
          ).normalize();
          let u = 0, v = 1; const nx = Math.abs(n.x), ny = Math.abs(n.y), nz = Math.abs(n.z);
          if (nx >= ny && nx >= nz) { u = 1; v = 2; } else if (ny >= nx && ny >= nz) { u = 0; v = 2; }
          const flat = [], hIdx = [];
          for (const p of outer) flat.push(p[u], p[v]);
          for (const h of hls) { hIdx.push(flat.length / 2); for (const p of h) flat.push(p[u], p[v]); }
          const indices = earcut(flat, hIdx), all = [...outer, ...hls.flat()];
          const pos = all.flatMap(p => [p[0], p[1], p[2]]), norm = all.flatMap(() => [n.x, n.y, n.z]);
          const g = new THREE.BufferGeometry();
          g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
          g.setAttribute('normal', new THREE.Float32BufferAttribute(norm, 3));
          g.setIndex(indices);
          const mesh = new THREE.Mesh(g, material); mesh.applyMatrix4(worldMat); scene.add(mesh);
        }
      }
    }
    if (s.shapes) for (const sub of s.shapes) await walk(sub, worldMat);
  };
  await walk(shape);
};

class JOTAssets {
    constructor(vfs) { this.vfs = vfs; this.cache = new Map(); this.main = null; }
    async parse(text) {
        let offset = 0; if (text[0] === '\n') offset++;
        while (offset < text.length) {
            if (text[offset] !== '=') break; offset++;
            const nl = text.indexOf('\n', offset), h = text.substring(offset, nl); offset = nl + 1;
            const sp = h.indexOf(' '), len = parseInt(h.substring(0, sp), 10), name = h.substring(sp + 1);
            const content = text.substring(offset, offset + len); offset += len;
            if (text[offset] === '\n') offset++;
            if (name.startsWith('files/')) this.main = JSON.parse(content);
            else if (name.startsWith('assets/text/')) this.cache.set(name.substring('assets/text/'.length), content);
        }
        return this.main;
    }
    async getText(id) {
        if (this.cache.has(id)) return this.cache.get(id);
        if (!this.vfs) return null;
        try {
            if (id.startsWith('vfs:/')) {
                const url = new URL(id.replace('vfs:/', 'vfs://'));
                const path = url.pathname.slice(1);
                const params = Object.fromEntries(url.searchParams.entries());
                const data = await this.vfs.readData(path, params);
                if (data) { this.cache.set(id, data); return data; }
            }
            const geo = await this.vfs.readData('geo/mesh', { id });
            if (geo) { this.cache.set(id, geo); return geo; }
        } catch (e) { console.warn('[Renderer] Asset resolution failed:', id, e); }
        return null;
    }
}

export async function renderJotToScene(vfs, data, scene) {
  const assets = new JOTAssets(vfs); 
  let shape;
  if (typeof data === 'string') {
    const t = data.trim();
    if (t.startsWith('=') || t.startsWith('\n=')) shape = await assets.parse(data);
    else if (t.includes('v ')) { const id = 'raw-' + Math.random().toString(36).slice(2); assets.cache.set(id, data); shape = { geometry: id }; }
  } else if (typeof data === 'object') shape = data;
  if (!shape) throw new Error('Invalid data');
  await buildMeshes({ assets, shape, scene });
  return shape;
}

const thumbCache = new Map(); // CID -> dataURL

export async function captureThumbnail(vfs, data, width = 256, height = 256) {
  // Use a simple hash of the data as a cache key if CID isn't available
  const cacheKey = typeof data === 'string' ? data.length + data.slice(0, 100) : JSON.stringify(data).length;
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
  canvas.width = width; canvas.height = height;
  const ctx = canvas.getContext('2d'), img = ctx.createImageData(width, height);
  for (let y = 0; y < height; y++) for (let x = 0; x < width; x++) {
    const src = (y * width + x) * 4, dst = ((height - y - 1) * width + x) * 4;
    for(let i=0; i<4; i++) img.data[dst+i] = buf[src+i];
  }
  ctx.putImageData(img, 0, 0);
  const dataUrl = canvas.toDataURL('image/png');
  thumbCache.set(cacheKey, dataUrl);
  return dataUrl;
}
