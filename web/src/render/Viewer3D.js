import * as THREE from 'three';
import { renderJotToScene } from './GeometryDecoder.js';

// ==========================================
// 1. Hitchhiker Shared WebGLRenderer Manager
// ==========================================
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
  
  sharedRenderer.toneMapping = THREE.ACESFilmicToneMapping;
  sharedRenderer.toneMappingExposure = 1.0;

  let lastTime = performance.now();
  const loop = () => {
    const now = performance.now();
    const dt = Math.min((now - lastTime) / 1000, 0.1);
    lastTime = now;

    if (activeViewport && sharedRenderer) {
      const { scene, camera, controls, onUpdate } = activeViewport;
      
      if (onUpdate) onUpdate(dt);
      if (controls && controls.enabled) controls.update();
      
      sharedRenderer.setClearColor(0x060709, 1); // Dark background matching theme
      sharedRenderer.render(scene, camera);
    }
    requestAnimationFrame(loop);
  };
  requestAnimationFrame(loop);

  return sharedRenderer;
}

export function activateViewport(id, container, scene, camera, controls, onUpdate) {
  const renderer = getSharedRenderer();
  if (activeViewport && activeViewport.id === id && renderer.domElement.parentElement === container) {
    activeViewport.onUpdate = onUpdate;
    return;
  }

  console.log(`[SharedRenderer] Activating Viewport: ${id}`);

  if (renderer.domElement.parentElement !== container) {
    container.innerHTML = ''; // Clear container first
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

  activeViewport = { id, container, scene, camera, controls, onUpdate };
}

export function captureSnapshot(scene, camera) {
  const renderer = getSharedRenderer();
  if (!scene || !camera) return null;

  renderer.setClearColor(0x060709, 1);
  renderer.render(scene, camera);
  const data = renderer.domElement.toDataURL('image/png');
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

// ==========================================
// 2. JotViewer3D Framework-Agnostic Viewer
// ==========================================
export class JotViewer3D {
  constructor(container, id = null) {
    this.container = container;
    this.id = id || Math.random().toString(36).substring(7);
    this.scene = new THREE.Scene();

    // Setup camera with Z-up standard coordinate space (from Viewport.jsx)
    this.camera = new THREE.PerspectiveCamera(45, container.clientWidth / container.clientHeight, 1, 20000);
    this.camera.position.set(100, 150, 200);
    this.camera.up.set(0, 0, 1); // Z is UP
    this.camera.lookAt(0, 0, 0);

    // Dynamic Orbit Controls placeholder
    const ControlsClass = window.THREE?.OrbitControls || THREE.OrbitControls;
    if (ControlsClass) {
      this.controls = new ControlsClass(this.camera, container);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.05;
      this.controls.rotateSpeed = 0.7;
      this.controls.enablePan = true;
      this.controls.screenSpacePanning = true;
    }

    this.activeMesh = null;
    this._setupLights();
    this.mount();
  }

  // Exact lighting coordinates & parameters from Viewport.jsx
  _setupLights() {
    // Ambient light is low to allow shadows and depth to show
    const ambient = new THREE.AmbientLight(0xffffff, 0.2);
    ambient.userData.isSystem = true;
    this.scene.add(ambient);

    // Hemisphere light provides sky/ground orientation gradients
    const hemi = new THREE.HemisphereLight(0xffffff, 0x000000, 0.4);
    hemi.userData.isSystem = true;
    this.scene.add(hemi);

    // Main light from front-top-right
    const mainLight = new THREE.DirectionalLight(0xffffff, 0.8);
    mainLight.position.set(200, 500, 300);
    mainLight.userData.isSystem = true;
    this.scene.add(mainLight);

    // Fill light from back-bottom-left to pop edges
    const fillLight = new THREE.DirectionalLight(0xffffff, 0.3);
    fillLight.position.set(-200, -100, -200);
    fillLight.userData.isSystem = true;
    this.scene.add(fillLight);
  }

  // Mounts WebGL renderer to target DOM container
  mount() {
    activateViewport(this.id, this.container, this.scene, this.camera, this.controls, null);
  }

  resize() {
    if (!this.container || !this.camera) return;
    const rect = this.container.getBoundingClientRect();
    const w = rect.width || this.container.clientWidth || 300;
    const h = rect.height || this.container.clientHeight || 200;
    
    this.camera.aspect = w / h;
    this.camera.updateProjectionMatrix();

    // Refresh active viewport bounds
    activateViewport(this.id, this.container, this.scene, this.camera, this.controls, null);
  }

  clear() {
    if (this.activeMesh) {
      this.scene.remove(this.activeMesh);
      this.activeMesh.traverse((child) => {
        if (child.geometry) child.geometry.dispose();
        if (child.material) {
          if (Array.isArray(child.material)) {
            child.material.forEach((mat) => mat.dispose());
          } else {
            child.material.dispose();
          }
        }
      });
      this.activeMesh = null;
    }
  }

  // Load static STL model
  loadSTL(url, STLLoaderClass) {
    if (!STLLoaderClass) {
      console.error('[JotViewer3D] STLLoaderClass is required');
      return;
    }
    const loader = new STLLoaderClass();
    loader.load(url, (geometry) => {
      this.clear();

      const material = new THREE.MeshPhongMaterial({
        color: 0x8a2be2,
        specular: 0x111111,
        shininess: 200,
        flatShading: true
      });

      const mesh = new THREE.Mesh(geometry, material);
      geometry.computeVertexNormals();
      geometry.center();

      // Camera auto zoom to centered STL model
      const boundingBox = new THREE.Box3().setFromObject(mesh);
      const size = boundingBox.getSize(new THREE.Vector3());
      const maxDim = Math.max(size.x, size.y, size.z) || 1;
      
      this.camera.position.set(maxDim, maxDim, maxDim * 2.5);
      this.camera.lookAt(0, 0, 0);
      if (this.controls) {
        this.controls.target.set(0, 0, 0);
        this.controls.update();
      }

      this.activeMesh = mesh;
      this.scene.add(this.activeMesh);
    }, undefined, (err) => {
      console.error('[JotViewer3D] Failed to load STL model:', err);
    });
  }

  // Parse and display native .jot shape geometry (supporting raw, json, and bundles)
  async loadJotGeometry(jotText) {
    this.clear();

    const group = new THREE.Group();
    try {
      await renderJotToScene(jotText, group);
    } catch (e) {
      console.error('[JotViewer3D] Failed to render JOT geometry:', e);
    }

    // Centering and camera zoom math (identical to Viewport.jsx)
    const boundingBox = new THREE.Box3().setFromObject(group);
    if (!boundingBox.isEmpty()) {
      const center = boundingBox.getCenter(new THREE.Vector3());
      const size = boundingBox.getSize(new THREE.Vector3());
      const maxDim = Math.max(size.x, size.y, size.z) || 1;

      this.camera.position.set(center.x + maxDim, center.y + maxDim, center.z + maxDim * 2.5);
      this.camera.lookAt(center);
      if (this.controls) {
        this.controls.target.copy(center);
        this.controls.update();
      }
    }

    this.activeMesh = group;
    this.scene.add(this.activeMesh);
  }

  destroy() {
    this.clear();
    if (activeViewport && activeViewport.id === this.id) {
      activeViewport = null;
    }
    this.container.innerHTML = '';
    this.container = null;
  }
}
