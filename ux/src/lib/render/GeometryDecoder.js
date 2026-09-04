import * as THREE from 'three';
import { computeBoundsTree, disposeBoundsTree, acceleratedRaycast } from 'three-mesh-bvh';
import { ratioToNumber } from '../ft.js';
import { JOTAssets, normalizeId } from './AssetManager.js';

import { decodeJotGeometry } from '../../../../web/src/render/GeometryDecoder.js';

// --- INITIALIZE BVH ---
THREE.BufferGeometry.prototype.computeBoundsTree = computeBoundsTree;
THREE.BufferGeometry.prototype.disposeBoundsTree = disposeBoundsTree;
THREE.Mesh.prototype.raycast = acceleratedRaycast;

export const decodeGeometry = decodeJotGeometry;

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
  const walk = async (s) => {
    const worldMat = decodeTf(s.tf);
    const shapeColor = toColor(s.tags);
    
    // Material & Texture Support
    const materialTag = s.tags?.material || s.tags?.tags?.material;
    let texture = null;
    if (materialTag && typeof materialTag === 'string') {
        texture = await assets.getTexture(materialTag);
        if (texture) {
            texture.wrapS = texture.wrapT = THREE.RepeatWrapping;
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
                side: THREE.FrontSide, 
                roughness: 0.4, 
                metalness: 0.2,
                transparent, 
                opacity,
                blending: transparent ? THREE.AdditiveBlending : THREE.NormalBlending,
                depthWrite: !transparent
            }));
            mesh.renderOrder = transparent ? 1 : 0;
            mesh.userData.isJot = true;

            // --- COMPUTE BVH ---
            g.computeBoundsTree();

            mesh.applyMatrix4(worldMat); scene.add(mesh);
            
            const edgesGeo = new THREE.EdgesGeometry(g, edgeThreshold);
            const edgesLine = new THREE.LineSegments(edgesGeo, new THREE.LineBasicMaterial({ 
                color: 0xffffff, 
                transparent: true, 
                opacity: isGhost ? 0.2 : (transparent ? 0.3 : 0.5),
                depthWrite: false
            }));
            edgesLine.renderOrder = transparent ? 1 : 0;
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
