import * as THREE from 'three';
import { decodeJotGeometry } from './GeometryDecoder.js';

export class JotViewer3D {
  constructor(container) {
    this.container = container;
    this.scene = new THREE.Scene();
    this.scene.background = new THREE.Color(0x060709);

    this.camera = new THREE.PerspectiveCamera(45, container.clientWidth / container.clientHeight, 0.1, 1000);
    this.camera.position.set(40, 40, 50);

    this.renderer = new THREE.WebGLRenderer({ antialias: true });
    this.renderer.setSize(container.clientWidth, container.clientHeight);
    this.renderer.setPixelRatio(window.devicePixelRatio);
    
    // Clear old elements and append the new canvas
    container.innerHTML = '';
    container.appendChild(this.renderer.domElement);

    // Dynamic Orbit Controls placeholder (loaded via instance or browser window)
    const ControlsClass = window.THREE?.OrbitControls || THREE.OrbitControls;
    if (ControlsClass) {
      this.controls = new ControlsClass(this.camera, this.renderer.domElement);
      this.controls.enableDamping = true;
      this.controls.dampingFactor = 0.05;
    }

    this.activeMesh = null;
    this._setupLights();
    this._animate();
  }

  _setupLights() {
    const ambientLight = new THREE.AmbientLight(0x222222);
    this.scene.add(ambientLight);

    const dirLight1 = new THREE.DirectionalLight(0xffffff, 0.85);
    dirLight1.position.set(1, 1, 1).normalize();
    this.scene.add(dirLight1);

    const dirLight2 = new THREE.DirectionalLight(0x444444, 0.5);
    dirLight2.position.set(-1, -1, -1).normalize();
    this.scene.add(dirLight2);
  }

  _animate = () => {
    if (!this.container) return; // Destroyed
    requestAnimationFrame(this._animate);
    if (this.controls) this.controls.update();
    this.renderer.render(this.scene, this.camera);
  };

  resize() {
    if (!this.container || !this.renderer) return;
    this.camera.aspect = this.container.clientWidth / this.container.clientHeight;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(this.container.clientWidth, this.container.clientHeight);
  }

  clear() {
    if (this.activeMesh) {
      this.scene.remove(this.activeMesh);
      // Recursively dispose geometries and materials
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

  // Load static STL model using a passed in STLLoader constructor (keeps loader modular)
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

      // Fit camera bounds
      const boundingBox = new THREE.Box3().setFromObject(mesh);
      const size = boundingBox.getSize(new THREE.Vector3());
      const maxDim = Math.max(size.x, size.y, size.z);
      this.camera.position.set(maxDim * 1.5, maxDim * 1.5, maxDim * 1.8);
      if (this.controls) this.controls.target.set(0, 0, 0);

      this.activeMesh = mesh;
      this.scene.add(this.activeMesh);
    }, undefined, (err) => {
      console.error('[JotViewer3D] Failed to load STL model:', err);
    });
  }

  // Parse and display native .jot shape geometry (vertices, triangles, segments)
  loadJotGeometry(jotText) {
    const { vertices, triangles, segments } = decodeJotGeometry(jotText);
    if (vertices.length === 0) return;

    this.clear();

    const group = new THREE.Group();

    // 1. Draw 3D Triangles
    if (triangles.length > 0) {
      const g = new THREE.BufferGeometry();
      g.setAttribute('position', new THREE.Float32BufferAttribute(vertices.flat(), 3));
      g.setIndex(triangles.flat());
      g.computeVertexNormals();

      const material = new THREE.MeshPhongMaterial({
        color: 0x8a2be2,
        specular: 0x111111,
        shininess: 200,
        flatShading: true,
        side: THREE.DoubleSide
      });

      const mesh = new THREE.Mesh(g, material);
      group.add(mesh);

      // Edge contours
      const edgesGeo = new THREE.EdgesGeometry(g, 15);
      const edgesLine = new THREE.LineSegments(edgesGeo, new THREE.LineBasicMaterial({
        color: 0xffffff,
        transparent: true,
        opacity: 0.4
      }));
      group.add(edgesLine);
    }

    // 2. Draw wireframe segments
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
          color: 0x8a2be2,
          linewidth: 2
        }));
        group.add(line);
      }
    }

    // Centering and Fit Camera
    const boundingBox = new THREE.Box3().setFromObject(group);
    const center = boundingBox.getCenter(new THREE.Vector3());
    group.position.sub(center);

    const size = boundingBox.getSize(new THREE.Vector3());
    const maxDim = Math.max(size.x, size.y, size.z);
    this.camera.position.set(maxDim * 1.5, maxDim * 1.5, maxDim * 1.8);
    if (this.controls) this.controls.target.set(0, 0, 0);

    this.activeMesh = group;
    this.scene.add(this.activeMesh);
  }

  destroy() {
    this.clear();
    this.container.innerHTML = '';
    this.container = null;
    this.renderer.dispose();
  }
}
