import * as THREE from 'three';
import { getSharedRenderer } from './SharedRenderer.js';
import { renderJotToScene } from './GeometryDecoder.js';

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
