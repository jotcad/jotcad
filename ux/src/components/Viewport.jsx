import { onMount, onCleanup, createEffect } from 'solid-js';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { renderJotToScene, registerViewport, unregisterViewport, initSharedRenderer, requestRender } from '../lib/three_utils';
import { vfs } from '../lib/blackboard';

export const Viewport = (props) => {
  let containerRef;
  let scene, camera, controls;
  const id = Math.random().toString(36).substring(7);
  let hasAutoZoomed = false;

  const init = () => {
    if (!containerRef) return;

    const width = containerRef.clientWidth || 100;
    const height = containerRef.clientHeight || 100;

    scene = new THREE.Scene();
    // scene.background = new THREE.Color(0x0f172a); // Removed to allow CSS background to show through

    camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 10000);
    camera.position.set(0, 0, 100);
    camera.up.set(0, 1, 0);
    camera.lookAt(0, 0, 0);

    const renderer = initSharedRenderer();

    controls = new OrbitControls(camera, containerRef);
    controls.enableDamping = true;
    controls.addEventListener('change', () => requestRender());

    registerViewport(id, containerRef, scene, camera, controls);

    // Lights
    scene.add(new THREE.AmbientLight(0xffffff, 0.8));
    const dl = new THREE.DirectionalLight(0xffffff, 1.0);
    dl.position.set(100, 200, 100);
    scene.add(dl);

    // Grid
    const grid = new THREE.GridHelper(100, 10);
    grid.rotateX(Math.PI / 2);
    grid.material.opacity = 0.2;
    grid.material.transparent = true;
    scene.add(grid);

    onCleanup(() => {
      unregisterViewport(id);
      controls.dispose();
    });
  };

  onMount(() => {
    init();
  });

  createEffect(async () => {
    const data = props.data;
    const threshold = props.edgeThreshold ?? 15;
    
    if (data && scene) {
      // Clear previous geometry (Meshes and LineSegments)
      const toRemove = [];
      scene.traverse(child => {
        if (child.type === 'Mesh' || child.type === 'LineSegments') {
          toRemove.push(child);
        }
      });
      for (const obj of toRemove) {
        if (obj.geometry) obj.geometry.dispose();
        if (obj.material) {
          if (Array.isArray(obj.material)) obj.material.forEach(m => m.dispose());
          else obj.material.dispose();
        }
        scene.remove(obj);
      }

      try {
        await renderJotToScene(vfs, data, scene, threshold);

        if (!hasAutoZoomed) {
          const box = new THREE.Box3().setFromObject(scene);
          if (!box.isEmpty()) {
            const center = box.getCenter(new THREE.Vector3());
            const size = box.getSize(new THREE.Vector3());
            const maxDim = Math.max(size.x, size.y, size.z) || 1;

            const distance = maxDim * 2;
            camera.position.set(center.x, center.y, center.z + distance);
            camera.lookAt(center);
            camera.up.set(0, 1, 0);

            controls.target.copy(center);
            controls.update();
            hasAutoZoomed = true;
          }
        }
        requestRender();
      } catch (e) {
        console.error('[Viewport] Render error:', e);
      }
    }
  });

  return (
    <div
      ref={containerRef}
      class="w-full h-full rounded-lg overflow-hidden bg-slate-950"
      onMouseDown={(e) => {}}
      onMouseMove={(e) => {}}
      onMouseUp={(e) => {}}
      onWheel={(e) => {}}
      onPointerDown={(e) => {}}
      onDblClick={(e) => {}}
      onContextMenu={(e) => {}}
    />
  );
};
