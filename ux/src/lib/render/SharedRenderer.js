import * as THREE from 'three';

// The "Hitchhiker" - One renderer to rule them all
let sharedRenderer = null;
let activeViewport = null; // { container, scene, camera, controls, id }

export function getSharedRenderer() {
  if (sharedRenderer) return sharedRenderer;
  sharedRenderer = new THREE.WebGLRenderer({ 
    antialias: true, 
    alpha: true,
    preserveDrawingBuffer: true,
    powerPreference: "high-performance"
  });
  sharedRenderer.setPixelRatio(window.devicePixelRatio);
  sharedRenderer.setSize(300, 200);
  sharedRenderer.setClearColor(0x00ffff, 1);
  sharedRenderer.autoClear = true;
  
  // Better color handling and realism
  sharedRenderer.toneMapping = THREE.ACESFilmicToneMapping;
  sharedRenderer.toneMappingExposure = 1.0;

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

export function activateViewport(id, container, scene, camera, controls) {
  const renderer = getSharedRenderer();
  if (activeViewport && activeViewport.id === id && renderer.domElement.parentElement === container) return;

  console.log(`[SharedRenderer] Activating Viewport: ${id}`);

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

export function captureSnapshot(scene, camera) {
  const renderer = getSharedRenderer();
  if (!scene || !camera) return null;

  renderer.setClearColor(0x008080, 1);
  renderer.render(scene, camera);
  const data = renderer.domElement.toDataURL('image/png');
  renderer.setClearColor(0x00ffff, 1);
  return data;
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

// Dummy exports for backward compatibility
export function initSharedRenderer() { return getSharedRenderer(); }
export function registerViewport() { return () => {}; }
export function unregisterViewport() { }
export function requestRender() { }
