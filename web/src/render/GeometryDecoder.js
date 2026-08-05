import * as THREE from 'three';

export function ratioToNumber(s) {
  if (typeof s === 'number') return s;
  if (typeof s !== 'string') return 0;
  
  // Handle multiple space-delimited ratios/numbers
  if (s.includes(' ')) {
    return s.trim().split(/\s+/).map(ratioToNumber);
  }

  const slash = s.indexOf('/');
  if (slash === -1) return parseFloat(s);

  const nStr = s.substring(0, slash);
  const dStr = s.substring(slash + 1);

  const n = parseFloat(nStr);
  const d = parseFloat(dStr);
  return d !== 0 ? n / d : 0;
}

// Normalize ID helper (from ux/src/lib/render/AssetManager.js)
export const normalizeId = (id) => {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => { acc[key] = params[key]; return acc; }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
};

// Decode custom .jot shape text representation format into vertices, triangles, and segments
export function decodeJotGeometry(text) {
  const vertices = [];
  const points = [];
  const triangles = [];
  const segments = [];
  const faces = [];
  
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
        if (vLine.length >= 3) {
          vertices.push([ratioToNumber(vLine[0]), ratioToNumber(vLine[1]), ratioToNumber(vLine[2])]);
        }
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
    } else {
      i++;
    }
  }
  return { vertices, points, triangles, segments, faces };
}

// JOT Archive Asset Loader (from ux/src/lib/render/AssetManager.js but with VFS fallback)
export class JOTAssets {
  constructor() {
    this.cache = new Map();
    this.main = null;
  }
  
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
    return null;
  }
}

export const decodeGeometry = decodeJotGeometry;

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

// Tag color
const toColor = (tags) => {
  if (!tags) return new THREE.Color(0x808080);
  const colorVal = tags.color || tags.tags?.color;
  if (colorVal) return new THREE.Color(colorVal);
  for (const key of Object.keys(tags)) if (key.startsWith('color:')) return new THREE.Color(key.substring(6));
  return new THREE.Color(0x808080);
};

// Recursive mesh construction matching primary UX shaders & edge outlines exactly
export const buildMeshes = async ({ assets, shape, scene, edgeThreshold = 15 }) => {
  const walk = async (s) => {
    const worldMat = decodeTf(s.tf);
    const shapeColor = toColor(s.tags);

    // Ghost Workflow Support
    const role = s.tags?.role || s.tags?.tags?.role;
    const isGhost = role === 'ghost' || role === 'gap';
    const tagOpacity = s.tags?.opacity !== undefined ? parseFloat(s.tags.opacity) : 
                       (s.tags?.tags?.opacity !== undefined ? parseFloat(s.tags.tags.opacity) : 1.0);
    const opacity = isGhost ? 0.15 : tagOpacity;
    const transparent = opacity < 1.0 || isGhost;

    if (s.geometry) {
      const text = await assets.getText(s.geometry);
      if (text) {
        const { vertices, triangles, segments } = decodeGeometry(text);
        if (vertices.length === 0) return;

        if (triangles.length > 0) {
          const g = new THREE.BufferGeometry();
          g.setAttribute('position', new THREE.Float32BufferAttribute(vertices.flat(), 3));
          g.setIndex(triangles.flat());
          g.computeVertexNormals();

          const material = new THREE.MeshPhongMaterial({
            color: isGhost ? 0x00ffff : shapeColor,
            specular: 0x111111,
            shininess: 200,
            flatShading: true,
            side: THREE.DoubleSide,
            transparent,
            opacity
          });

          const mesh = new THREE.Mesh(g, material);
          mesh.userData.isJot = true;
          mesh.applyMatrix4(worldMat);
          scene.add(mesh);

          const edgesGeo = new THREE.EdgesGeometry(g, edgeThreshold);
          const edgesLine = new THREE.LineSegments(edgesGeo, new THREE.LineBasicMaterial({
            color: isGhost ? 0x00ffff : 0xffffff,
            transparent: true,
            opacity: isGhost ? 0.2 : 0.5
          }));
          edgesLine.userData.isJot = true;
          edgesLine.applyMatrix4(worldMat);
          scene.add(edgesLine);
        }

        if (segments.length > 0) {
          const pos = [];
          for (const seg of segments) {
            const v0 = vertices[seg[0]];
            const v1 = vertices[seg[1]];
            if (v0 && v1) pos.push(...v0, ...v1);
          }
          if (pos.length > 0) {
            const g = new THREE.BufferGeometry();
            g.setAttribute('position', new THREE.Float32BufferAttribute(pos, 3));
            const line = new THREE.LineSegments(g, new THREE.LineBasicMaterial({
              color: isGhost ? 0x00ffff : shapeColor,
              linewidth: 2,
              transparent,
              opacity
            }));
            line.userData.isJot = true;
            line.applyMatrix4(worldMat);
            scene.add(line);
          }
        }
      }
    }
    if (s.components) {
      for (const sub of s.components) {
        await walk(sub);
      }
    }
  };
  await walk(shape);
};

// Main entry point for .jot decoding (supporting raw text, json shapes, and ZFS archive bundles)
export async function renderJotToScene(data, scene, edgeThreshold = 15) {
  const assets = new JOTAssets();
  let shape = null;
  let textData = data;

  if (data instanceof Uint8Array) {
    textData = new TextDecoder().decode(data);
  }

  if (typeof textData === 'string') {
    const t = textData.trim();
    if (t.startsWith('=') || t.startsWith('\n=')) {
      shape = await assets.parse(textData);
    } else if (t.startsWith('{')) {
      shape = JSON.parse(t);
    } else {
      // Raw V/F/T/S single geometry fallback
      const id = 'raw-geom';
      assets.cache.set(id, textData);
      shape = { geometry: id };
    }
  } else if (typeof textData === 'object') {
    shape = textData;
  }

  if (shape) {
    await buildMeshes({ assets, shape, scene, edgeThreshold });
  }
  return shape;
}
