import * as THREE from 'three';
import { ratioToNumber } from '../ft.js';
import { JOTAssets, normalizeId } from './AssetManager.js';

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
    
    // Material & Texture Support
    const materialTag = s.tags?.material || s.tags?.tags?.material;
    let texture = null;
    if (materialTag && typeof materialTag === 'string') {
        // Heuristic for CID: 64 hex chars or containing path separators
        if (materialTag.length === 64 || materialTag.includes('/') || materialTag.includes('?')) {
            texture = await assets.getTexture(materialTag);
            if (texture) {
                texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
            }
        }
    }

    // Ghost Workflow Support
    const role = s.tags?.role || s.tags?.tags?.role;
    const isGhost = role === 'ghost' || role === 'gap';
    const tagOpacity = s.tags?.opacity !== undefined ? parseFloat(s.tags.opacity) : 
                       (s.tags?.tags?.opacity !== undefined ? parseFloat(s.tags.tags.opacity) : 1.0);
    const opacity = isGhost ? 0.3 : tagOpacity;
    const transparent = opacity < 1.0;

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

            if (texture) {
                // Simple box/planar projection for UVs if they are missing
                const pos = vertices.flat();
                const uvs = new Float32Array((pos.length / 3) * 2);
                for (let i = 0; i < pos.length / 3; i++) {
                    // Use a 100-unit scale for texture repeats
                    uvs[i * 2] = pos[i * 3] / 100;
                    uvs[i * 2 + 1] = pos[i * 3 + 1] / 100;
                }
                g.setAttribute('uv', new THREE.BufferAttribute(uvs, 2));
            }

            const mesh = new THREE.Mesh(g, new THREE.MeshStandardMaterial({ 
                color: texture ? 0xffffff : shapeColor,
                map: texture,
                side: THREE.DoubleSide, roughness: 0.5, metalness: 0.1,
                transparent, opacity 
            }));
            mesh.userData.isJot = true;
            mesh.applyMatrix4(worldMat); scene.add(mesh);
            
            const edgesGeo = new THREE.EdgesGeometry(g, edgeThreshold);
            const edgesLine = new THREE.LineSegments(edgesGeo, new THREE.LineBasicMaterial({ 
                color: 0xffffff, transparent: true, opacity: isGhost ? 0.2 : 0.5 
            }));
            edgesLine.userData.isJot = true;
            edgesLine.applyMatrix4(worldMat); scene.add(edgesLine);
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
                    color: shapeColor, linewidth: 2, transparent, opacity
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

export async function renderJotToScene(vfs, data, scene, edgeThreshold = 15) {
  const assets = new JOTAssets(vfs);
  let shape;
  let textData = data;
  if (data instanceof Uint8Array) textData = new TextDecoder().decode(data);
  
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
          const data = await blackboard.readCIDData(s.geometry);
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
