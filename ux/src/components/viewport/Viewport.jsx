import { onMount, onCleanup, createEffect, createSignal, Show, untrack } from 'solid-js';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { activateViewport, captureSnapshot, createLabel } from '../../lib/render/SharedRenderer';
import { renderJotToScene } from '../../lib/render/GeometryDecoder';
import { vfs } from '../../lib/blackboard';

export const Viewport = (props) => {
  let containerRef;
  let scene, camera, controls;
  const id = Math.random().toString(36).substring(7);
  
  const [snapshot, setSnapshot] = createSignal(null);
  const [isActive, setIsActive] = createSignal(false);
  const [hasAutoZoomed, setHasAutoZoomed] = createSignal(false);

  const init = () => {
    if (!containerRef) return;
    const width = containerRef.clientWidth || 300;
    const height = containerRef.clientHeight || 200;

    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(45, width / height, 1, 20000);
    camera.position.set(100, 150, 200);
    camera.lookAt(0, 0, 0);

    controls = new OrbitControls(camera, containerRef);
    controls.enableDamping = true;
    controls.dampingFactor = 0.05;
    controls.rotateSpeed = 0.7; // Slightly slower for touch precision
    controls.enablePan = true;
    controls.screenSpacePanning = true;

    // --- RE-SIZE TRACKING ---
    const resizeObserver = new ResizeObserver(() => {
        if (!containerRef || !camera) return;
        const w = containerRef.clientWidth || 300;
        const h = containerRef.clientHeight || 200;
        camera.aspect = w / h;
        camera.updateProjectionMatrix();
        
        // If active, the shared renderer will handle the actual canvas size update
        // but we trigger a snapshot update if inactive
        if (!isActive()) {
            setTimeout(() => {
                const snap = captureSnapshot(scene, camera);
                if (snap) setSnapshot(snap);
            }, 50);
        }
    });
    resizeObserver.observe(containerRef);
    onCleanup(() => resizeObserver.disconnect());

    // --- SYSTEM OBJECTS (Persistent) ---
    const ambient = new THREE.AmbientLight(0xffffff, 0.8);
    ambient.userData.isSystem = true;
    scene.add(ambient);

    const dl = new THREE.DirectionalLight(0xffffff, 1.0);
    dl.position.set(200, 500, 300);
    dl.userData.isSystem = true;
    scene.add(dl);

    updateGridAndLabels({ x: 100, y: 100 });
    
    // Capture initial empty state to remove "Click to Initialize"
    const snap = captureSnapshot(scene, camera);
    setSnapshot(snap);
  };

  onMount(() => {
    init();
  });

  onCleanup(() => {
    if (controls) controls.dispose();
  });

  const updateGridAndLabels = (maxPos) => {
    if (!scene) return;

    // 1. Clear existing dynamic system objects (Grid, Labels)
    const toRemove = [];
    scene.traverse(child => {
      if (child.userData.isSystem && (child.type === 'GridHelper' || child.type === 'AxesHelper' || child.type === 'Sprite')) {
        toRemove.push(child);
      }
    });
    toRemove.forEach(obj => scene.remove(obj));

    // 2. Calculate optimal grid size (nearest 100 to keep 100mm marks aligned)
    const size = Math.ceil(Math.max(maxPos.x, maxPos.y, 100) / 100) * 100;

    // Minor Grid (10mm / 1cm)
    const minorGrid = new THREE.GridHelper(size, size / 10, 0x444444, 0x444444);
    minorGrid.rotation.x = Math.PI / 2;
    minorGrid.position.set(size / 2, size / 2, 0.1); 
    minorGrid.material.transparent = true;
    minorGrid.material.opacity = 0.2;
    minorGrid.material.depthTest = false;
    minorGrid.material.depthWrite = false;
    minorGrid.renderOrder = 1000;
    minorGrid.userData.isSystem = true;
    scene.add(minorGrid);

    // Major Grid (100mm / 10cm) - Color Coded Cyan
    const majorGrid = new THREE.GridHelper(size, size / 100, 0x00ffff, 0x00ffff);
    majorGrid.rotation.x = Math.PI / 2;
    majorGrid.position.set(size / 2, size / 2, 0.11);
    majorGrid.material.transparent = true;
    majorGrid.material.opacity = 0.4;
    majorGrid.material.depthTest = false;
    majorGrid.material.depthWrite = false;
    majorGrid.renderOrder = 1001;
    majorGrid.userData.isSystem = true;
    scene.add(majorGrid);

    // Labels at 10mm marks
    for (let i = 10; i <= size; i += 10) {
      const isMajor = i % 100 === 0;
      const color = isMajor ? '#00ffff' : '#888888';

      const xLabel = createLabel(i.toString(), color, isMajor ? 12 : 10);
      xLabel.position.set(i, -8, 0.2);
      xLabel.material.depthTest = false;
      xLabel.renderOrder = 1002;
      xLabel.userData.isSystem = true;
      scene.add(xLabel);

      const yLabel = createLabel(i.toString(), color, isMajor ? 12 : 10);
      yLabel.position.set(-10, i, 0.2);
      yLabel.material.depthTest = false;
      yLabel.renderOrder = 1002;
      yLabel.userData.isSystem = true;
      scene.add(yLabel);
    }
  };

  const handleActivate = () => {
    if (isActive()) return;
    if (!scene) init();
    
    setIsActive(true);
    activateViewport(id, containerRef, scene, camera, controls);
    
    const deactivate = (e) => {
      if (containerRef && !containerRef.contains(e.target)) {
        // Capture the final state before hiding the renderer
        const snap = captureSnapshot(scene, camera);
        setSnapshot(snap);

        setIsActive(false);
        window.removeEventListener('pointerdown', deactivate);
      }
    };
    window.addEventListener('pointerdown', deactivate);
  };

  // 1. Geometry Effect (Only runs when data/threshold changes)
  createEffect(async () => {
    const data = props.data;
    const threshold = props.edgeThreshold ?? 15;
    
    if (!scene) init();
    if (!scene || !data) return;

    // Clear previous JOT geometry
    const toRemove = [];
    scene.traverse(child => {
      if (!child.userData.isSystem && (child.type === 'Mesh' || child.type === 'LineSegments')) {
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

      const box = new THREE.Box3();
      scene.traverse(obj => { if(obj.userData.isJot) box.expandByObject(obj); });
      
      if (!box.isEmpty()) {
        const center = box.getCenter(new THREE.Vector3());
        const geoSize = box.getSize(new THREE.Vector3());
        const maxDim = Math.max(geoSize.x, geoSize.y, geoSize.z) || 1;
        updateGridAndLabels(box.max);

        const alreadyZoomed = untrack(hasAutoZoomed);
        const isDefaultCam = camera.position.x === 100 && camera.position.y === 150 && camera.position.z === 200;

        if (!alreadyZoomed && isDefaultCam) {
          if (isFinite(center.x) && isFinite(center.y) && isFinite(center.z) && isFinite(maxDim)) {
            camera.position.set(center.x + maxDim, center.y + maxDim, center.z + maxDim * 2.5);
            camera.lookAt(center);
            controls.target.copy(center);
            controls.update();
            untrack(() => setHasAutoZoomed(true));
          }
        }
      } else {
        updateGridAndLabels({ x: 100, y: 100 });
      }

      // If we are NOT active, update the snapshot now
      if (!untrack(isActive)) {
        setTimeout(() => {
          const snap = captureSnapshot(scene, camera);
          if (snap) setSnapshot(snap);
        }, 50); // Give it a few frames to settle
      }
    } catch (e) { console.error('[Viewport] Render error:', e); }
  });

  // 2. Activation Sync (No geometry changes)
  createEffect(() => {
    const active = isActive();
    if (active) {
       // Ensure renderer is sized correctly upon activation
       const rect = containerRef.getBoundingClientRect();
       const width = rect.width || 300;
       const height = rect.height || 200;
       camera.aspect = width / height;
       camera.updateProjectionMatrix();
    }
  });

  const handleWheel = (e) => {
    e.stopPropagation();
  };

  return (
    <div class="relative w-full h-full">
      <button 
        onClick={() => setHasAutoZoomed(false)}
        class="absolute top-2 left-2 z-10 px-2 py-1 text-[10px] bg-cyan-500/10 hover:bg-cyan-500/20 text-cyan-400 rounded border border-cyan-400/20 transition-all font-black uppercase tracking-tighter"
      >
        Reset Camera
      </button>
      <div
        ref={containerRef}
        class="viewport-container w-full h-full rounded-lg overflow-hidden bg-slate-950 relative cursor-pointer"
        onPointerDown={handleActivate}
        onWheel={handleWheel}
      >
        <Show when={snapshot() && !isActive()}>
          <img src={snapshot()} class="w-full h-full object-contain pointer-events-none" />
        </Show>
        
        <Show when={!snapshot() && !isActive()}>
          <div class="w-full h-full flex items-center justify-center bg-slate-900">
              <span class="text-[9px] font-black uppercase text-white/20">Click to Initialize</span>
          </div>
        </Show>
      </div>
    </div>
  );
};
