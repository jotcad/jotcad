import { onMount, onCleanup, createEffect } from 'solid-js';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { renderJotToScene, registerViewport, unregisterViewport, initSharedRenderer, requestRender } from '../lib/three_utils';
import { vfs } from '../lib/blackboard';

export const Viewport = (props) => {
  let containerRef;
  let scene, camera, controls;
  const id = Math.random().toString(36).substring(7);

  const init = () => {
    if (!containerRef) return;

    const width = containerRef.clientWidth || 100;
    const height = containerRef.clientHeight || 100;

    scene = new THREE.Scene();
    scene.background = new THREE.Color(0x0f172a); // Slate-950

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
    if (data && scene) {
      // Clear previous geometry
      while (scene.children.length > 0) {
        const obj = scene.children[0];
        if (obj.type === 'Mesh' || obj.type === 'LineSegments') {
          obj.geometry.dispose();
          obj.material.dispose();
        }
        scene.remove(obj);
      }

      scene.add(new THREE.AmbientLight(0xffffff, 0.6));
      const dl = new THREE.DirectionalLight(0xffffff, 0.8);
      dl.position.set(100, 200, 100);
      scene.add(dl);

      try {
        await renderJotToScene(vfs, data, scene);

        const box = new THREE.Box3().setFromObject(scene);
        const center = box.getCenter(new THREE.Vector3());
        const size = box.getSize(new THREE.Vector3());
        const maxDim = Math.max(size.x, size.y, size.z) || 1;

        const distance = maxDim * 2;
        camera.position.set(center.x, center.y, center.z + distance);
        camera.lookAt(center);
        camera.up.set(0, 1, 0);

        controls.target.copy(center);
        controls.update();
        requestRender();
      } catch (e) {
        console.error('[Viewport] Render error:', e);
      }
    }
  });

  return (
    <div
      ref={containerRef}
      class="w-full h-full rounded-lg overflow-hidden"
      onMouseDown={(e) => e.stopPropagation()}
      onMouseMove={(e) => e.stopPropagation()}
      onMouseUp={(e) => e.stopPropagation()}
      onWheel={(e) => e.stopPropagation()}
      onPointerDown={(e) => e.stopPropagation()}
      onDblClick={(e) => e.stopPropagation()}
      onContextMenu={(e) => e.stopPropagation()}
    />
  );
};
