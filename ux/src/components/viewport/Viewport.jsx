import { onMount, onCleanup, createEffect, createSignal, Show, untrack } from 'solid-js';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { activateViewport, captureSnapshot, createLabel } from '../../lib/render/SharedRenderer';
import { renderJotToScene } from '../../lib/render/GeometryDecoder';
import { vfs } from '../../lib/blackboard';
import { viewportSettings } from '../../lib/state/SystemState';

export const Viewport = (props) => {
  let containerRef;
  let scene, camera, controls;
  const id = Math.random().toString(36).substring(7);
  
  const [snapshot, setSnapshot] = createSignal(null);
  const [isActive, setIsActive] = createSignal(false);
  const [hasAutoZoomed, setHasAutoZoomed] = createSignal(false);
  const [viewMode, setViewMode] = createSignal('orbit'); // 'orbit' or 'walk'
  const [isFlying, setIsFlying] = createSignal(true);
  const [speedScale, setSpeedScale] = createSignal(1.0);
  const [camPos, setCamPos] = createSignal({ x: 0, y: 0, z: 0 });
  const [lookDir, setLookDir] = createSignal({ x: 0, y: 0, z: 0 });
  const [geoBox, setGeoBox] = createSignal({ center: {x:0,y:0,z:0}, size: {x:0,y:0,z:0} });
  const [meshCount, setMeshCount] = createSignal(0);
  const [groundHgt, setGroundHgt] = createSignal(0);
  let jotObjects = [];

  // --- MOVEMENT STATE ---
  const keys = { w: false, a: false, s: false, d: false, space: false, shift: false };
  const velocity = new THREE.Vector3();
  const baseMoveSpeed = 1500.0; // ~1.5 m/s human walk
  const baseFlySpeed = 2500.0;  
  
  // Derived constants that scale with speedScale
  const getEyeHeight = () => 1650 * speedScale();
  const getGravity = () => 9800 * speedScale(); // 9.8 m/s^2
  const getJumpForce = () => 3000 * speedScale();
  
  let isGrounded = false;
  const raycaster = new THREE.Raycaster();
  const down = new THREE.Vector3(0, 0, -1);

  // Sync renderer whenever mode changes
  createEffect(() => {
    const mode = viewMode();
    if (isActive()) {
        if (mode === 'walk') {
            controls.enabled = false;
            activateViewport(id, containerRef, scene, camera, controls, updateWalk);
        } else {
            controls.enabled = true;
            activateViewport(id, containerRef, scene, camera, controls, null);
        }
    }
  });

  // Scale camera near plane with movement speed
  createEffect(() => {
    const scale = speedScale();
    if (camera) {
        camera.near = Math.max(0.01, scale * 1.0);
        camera.updateProjectionMatrix();
    }
  });

  const onKeyDown = (e) => {
    const key = e.key.toLowerCase();
    if (key === 'v') { toggleViewMode(); return; }
    
    // Q/E Speed Scaling
    if (key === 'q') { setSpeedScale(s => Math.max(0.01, s * 0.9)); e.preventDefault(); return; }
    if (key === 'e') { setSpeedScale(s => Math.min(100, s * 1.1)); e.preventDefault(); return; }

    if (viewMode() === 'orbit' || !isActive()) return;
    
    // Prevent WASD/Space/Shift from scrolling or triggering browser shortcuts
    if (['w', 'a', 's', 'd', ' ', 'shift'].includes(key)) {
        e.preventDefault();
    }

    if (key === 'w') keys.w = true;
    if (key === 'a') keys.a = true;
    if (key === 's') keys.s = true;
    if (key === 'd') keys.d = true;
    if (key === ' ') keys.space = true;
    if (e.key === 'Shift') keys.shift = true;
    if (key === 'f') setIsFlying(!isFlying());
  };

  const onKeyUp = (e) => {
    const key = e.key.toLowerCase();
    if (key === 'w') keys.w = false;
    if (key === 'a') keys.a = false;
    if (key === 's') keys.s = false;
    if (key === 'd') keys.d = false;
    if (key === ' ') keys.space = false;
    if (e.key === 'Shift') keys.shift = false;
  };

  const onMouseMove = (e) => {
    if (viewMode() === 'orbit' || !isActive()) return;
    if (document.pointerLockElement !== containerRef) return;

    // Standard FPS rotation (Z-up orientation for JotCAD)
    const sensitivity = 0.002;
    camera.rotation.order = 'ZXY'; // Yaw (Z), Pitch (X), Roll (Y)
    camera.rotation.z -= e.movementX * sensitivity;
    camera.rotation.x -= e.movementY * sensitivity;
    
    // Clamp pitch to allow nearly 180 degree vertical look (Horizon is PI/2)
    camera.rotation.x = Math.max(0.1, Math.min(Math.PI - 0.1, camera.rotation.x));
  };

  const updateWalk = (dt) => {
    if (viewMode() !== 'walk' || !scene || !isActive()) return;

    const dir = new THREE.Vector3();
    const forward = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
    const right = new THREE.Vector3(1, 0, 0).applyQuaternion(camera.quaternion);
    const currentScale = speedScale();

    if (isFlying()) {
        if (keys.w) dir.add(forward);
        if (keys.s) dir.sub(forward);
        if (keys.a) dir.sub(right);
        if (keys.d) dir.add(right);
        if (keys.space) dir.z += 1;
        if (keys.shift) dir.z -= 1;
        
        if (dir.lengthSq() > 0) {
            velocity.copy(dir.normalize().multiplyScalar(baseFlySpeed * currentScale * dt));
            camera.position.add(velocity);
        }
    } else {
        // Walk mode (Locked to XY plane for movement)
        forward.z = 0; 
        if (forward.lengthSq() > 0.001) forward.normalize();
        
        right.z = 0; 
        if (right.lengthSq() > 0.001) right.normalize();

        if (keys.w) dir.add(forward);
        if (keys.s) dir.sub(forward);
        if (keys.a) dir.sub(right);
        if (keys.d) dir.add(right);

        const targetVelX = dir.x * baseMoveSpeed * currentScale;
        const targetVelY = dir.y * baseMoveSpeed * currentScale;
        
        // Horizontal inertia (Speed per second)
        velocity.x += (targetVelX - velocity.x) * 10 * dt;
        velocity.y += (targetVelY - velocity.y) * 10 * dt;

        // Gravity (Acceleration)
        velocity.z -= getGravity() * dt;

        if (isGrounded && keys.space) {
            velocity.z = getJumpForce();
            isGrounded = false;
        }

        camera.position.x += velocity.x * dt;
        camera.position.y += velocity.y * dt;
        camera.position.z += velocity.z * dt;

        // --- FOOTPRINT PHYSICS (4-point raycast) ---
        // Player has a radius of 300mm (scaled)
        const radius = 300 * currentScale;
        const offsets = [
            new THREE.Vector3(0, 0, 0), // Center
            new THREE.Vector3(radius, 0, 0),
            new THREE.Vector3(-radius, 0, 0),
            new THREE.Vector3(0, radius, 0),
            new THREE.Vector3(0, -radius, 0)
        ];

        let maxGroundZ = 0;
        const meshObjects = jotObjects.filter(obj => obj.type === 'Mesh');

        for (const offset of offsets) {
            const rayOrigin = camera.position.clone().add(offset);
            rayOrigin.z += 1000 * currentScale; // Start 1m (scaled) above
            
            raycaster.set(rayOrigin, down);
            const intersects = raycaster.intersectObjects(meshObjects, true);
            if (intersects.length > 0) {
                maxGroundZ = Math.max(maxGroundZ, intersects[0].point.z);
            }
        }

        const floor = maxGroundZ + getEyeHeight();
        setGroundHgt(camera.position.z - maxGroundZ);

        if (camera.position.z <= floor) {
            camera.position.z = floor;
            velocity.z = 0;
            isGrounded = true;
        } else {
            isGrounded = false;
        }
    }
    if (camera) {
        setCamPos({ x: camera.position.x, y: camera.position.y, z: camera.position.z });
        const d = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
        setLookDir({ x: d.x, y: d.y, z: d.z });
    }
  };

  const safeRequestLock = () => {
    if (!containerRef || !containerRef.isConnected) return;
    
    // Pointer lock must be triggered by a user gesture.
    // We use a small delay to ensure Solid has finished any DOM patching.
    requestAnimationFrame(() => {
        if (!containerRef || !containerRef.isConnected) return;
        try {
            const promise = containerRef.requestPointerLock();
            if (promise && promise.catch) {
                promise.catch(err => {
                    // Ignore "user denied" or "already locked" errors
                    if (err.name !== 'NotAllowedError' && err.name !== 'SecurityError') {
                        console.warn('[Viewport] Pointer lock failed:', err);
                    }
                });
            }
        } catch (e) {
            console.warn('[Viewport] Pointer lock exception:', e);
        }
    });
  };

  const toggleViewMode = () => {
    const newMode = viewMode() === 'orbit' ? 'walk' : 'orbit';
    
    if (newMode === 'walk') {
        const box = geoBox();
        const maxDim = Math.max(box.size.x, box.size.y, box.size.z, 10);
        
        // Target Speed = 1% of maxDim
        const newScale = (maxDim * 0.01) / baseMoveSpeed;
        setSpeedScale(newScale);

        setViewMode(newMode);
        controls.enabled = false;
        setIsFlying(false); // Default to walking
        if (isActive()) safeRequestLock();
        
        // Orient camera forward along +Y axis (Z-up)
        camera.rotation.order = 'ZXY';
        camera.rotation.set(1.8, 0, 0, 'ZXY'); 

        // Immediate snap to ground at (0,0)
        if (scene) {
            // Raycast from high above 0,0 downwards
            raycaster.set(new THREE.Vector3(0, 0, 10000), down);
            const intersects = raycaster.intersectObjects(jotObjects, true);
            let groundZ = 0;
            if (intersects.length > 0) groundZ = intersects[0].point.z;
            
            // USE LOCAL newScale for immediate height calculation
            const currentEyeHgt = 1650 * newScale;
            camera.position.set(0, 0, groundZ + currentEyeHgt);
            velocity.set(0, 0, 0);
        }
        
        const d = new THREE.Vector3(0, 0, -1).applyQuaternion(camera.quaternion);
        setLookDir({ x: d.x, y: d.y, z: d.z });
        setCamPos({ x: camera.position.x, y: camera.position.y, z: camera.position.z });
    } else {
        setViewMode(newMode);
        controls.enabled = true;
        try { document.exitPointerLock(); } catch(e) {}
    }
  };

  const currentStepDist = () => {
    const base = isFlying() ? baseFlySpeed : baseMoveSpeed;
    return (base * speedScale()).toFixed(1);
  };

  const init = () => {
    if (!containerRef) return;
    const width = containerRef.clientWidth || 300;
    const height = containerRef.clientHeight || 200;

    scene = new THREE.Scene();
    camera = new THREE.PerspectiveCamera(45, width / height, 1, 20000);
    camera.position.set(100, 150, 200);
    camera.up.set(0, 0, 1); // Ensure Z is UP
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
    // Ambient is low to allow shadows and depth to show
    const ambient = new THREE.AmbientLight(0xffffff, 0.2);
    ambient.userData.isSystem = true;
    scene.add(ambient);

    // Hemisphere light provides a subtle sky/ground gradient for orientation
    const hemi = new THREE.HemisphereLight(0xffffff, 0x000000, 0.4);
    hemi.userData.isSystem = true;
    scene.add(hemi);

    // Main light from front-top-right
    const mainLight = new THREE.DirectionalLight(0xffffff, 0.8);
    mainLight.position.set(200, 500, 300);
    mainLight.userData.isSystem = true;
    scene.add(mainLight);

    // Fill light from back-bottom-left to pop the edges
    const fillLight = new THREE.DirectionalLight(0xffffff, 0.3);
    fillLight.position.set(-200, -100, -200);
    fillLight.userData.isSystem = true;
    scene.add(fillLight);

    updateGridAndLabels({ x: 100, y: 100 });
    
    // Capture initial empty state to remove "Click to Initialize"
    const snap = captureSnapshot(scene, camera);
    setSnapshot(snap);
  };

  onMount(() => {
    init();
    window.addEventListener('keydown', onKeyDown);
    window.addEventListener('keyup', onKeyUp);
    window.addEventListener('mousemove', onMouseMove);
  });

  onCleanup(() => {
    if (controls) controls.dispose();
    window.removeEventListener('keydown', onKeyDown);
    window.removeEventListener('keyup', onKeyUp);
    window.removeEventListener('mousemove', onMouseMove);
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

    const showGrid = viewportSettings().hitchhikerGrid;

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
    minorGrid.visible = showGrid;
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
    majorGrid.visible = showGrid;
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
      xLabel.visible = showGrid;
      scene.add(xLabel);

      const yLabel = createLabel(i.toString(), color, isMajor ? 12 : 10);
      yLabel.position.set(-10, i, 0.2);
      yLabel.material.depthTest = false;
      yLabel.renderOrder = 1002;
      yLabel.userData.isSystem = true;
      yLabel.visible = showGrid;
      scene.add(yLabel);
    }
  };

  const handleActivate = () => {
    if (isActive()) {
        if (viewMode() === 'walk') safeRequestLock();
        return;
    }
    if (!scene) init();
    
    setIsActive(true);
    activateViewport(id, containerRef, scene, camera, controls, viewMode() === 'walk' ? updateWalk : null);
    
    // If we are already in walk mode, request lock on first click
    if (viewMode() === 'walk') {
        controls.enabled = false;
        safeRequestLock();
    }
    
    const deactivate = (e) => {
      if (containerRef && !containerRef.contains(e.target)) {
        // Capture the final state before hiding the renderer
        const snap = captureSnapshot(scene, camera);
        setSnapshot(snap);

        setIsActive(false);
        if (viewMode() === 'walk') {
            setViewMode('orbit');
            controls.enabled = true;
            try { document.exitPointerLock(); } catch(e) {}
        }
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
      let count = 0;
      jotObjects = [];
      scene.traverse(obj => { 
        if(obj.userData.isJot) {
            box.expandByObject(obj); 
            if (obj.type === 'Mesh') {
                count++;
                jotObjects.push(obj);
            }
        }
      });
      
      setMeshCount(count);
      if (!box.isEmpty()) {
        const center = box.getCenter(new THREE.Vector3());
        const size = box.getSize(new THREE.Vector3());
        setGeoBox({
            center: { x: center.x, y: center.y, z: center.z },
            size: { x: size.x, y: size.y, z: size.z }
        });

        const geoSize = size;
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

  createEffect(() => {
    const showGrid = viewportSettings().hitchhikerGrid;
    if (scene) {
        scene.traverse(child => {
            if (child.userData.isSystem && (child.type === 'GridHelper' || child.type === 'Sprite')) {
                child.visible = showGrid;
            }
        });
    }
  });

  const handleWheel = (e) => {
    if (viewMode() === 'walk') {
        e.preventDefault();
        e.stopPropagation();
        const factor = e.deltaY > 0 ? 0.9 : 1.1;
        setSpeedScale(Math.max(0.01, Math.min(100, speedScale() * factor)));
        return;
    }
    e.stopPropagation();
  };

  return (
    <div class="relative w-full h-full">
      <div class="absolute top-2 left-2 z-10 flex gap-2">
        <button 
            onClick={() => setHasAutoZoomed(false)}
            class="px-2 py-1 text-[10px] bg-cyan-500/10 hover:bg-cyan-500/20 text-cyan-400 rounded border border-cyan-400/20 transition-all font-black uppercase tracking-tighter"
        >
            Reset Camera
        </button>
        <button 
            onClick={(e) => { e.stopPropagation(); toggleViewMode(); }}
            class={`px-2 py-1 text-[10px] rounded border transition-all font-black uppercase tracking-tighter ${
                viewMode() === 'walk' 
                ? 'bg-amber-500/20 border-amber-400 text-amber-400' 
                : 'bg-cyan-500/10 border-cyan-400/20 text-cyan-400'
            }`}
        >
            {viewMode() === 'walk' ? (isFlying() ? `Fly x${speedScale().toFixed(4)} (${currentStepDist()}mm/s)` : `Walk x${speedScale().toFixed(4)} (${currentStepDist()}mm/s)`) : 'Mode: Orbit'}
        </button>
        <Show when={viewMode() === 'walk'}>
            <div class="px-2 py-1 text-[9px] bg-black/60 text-white/40 rounded border border-white/10 font-mono flex gap-2 items-center">
                <span>KEYS: WASD (MOVE) | SPACE/SHIFT (UP/DN) | Q/E (SPEED)</span>
                <span class="text-white border-l border-white/10 pl-2">
                    HGT: {groundHgt().toFixed(1)}mm
                </span>
                <span class="text-cyan-400 border-l border-white/10 pl-2">
                    POS: {camPos().x.toFixed(0)}, {camPos().y.toFixed(0)}, {camPos().z.toFixed(0)}
                </span>
                <span class="text-amber-400 border-l border-white/10 pl-2">
                    DIR: {lookDir().x.toFixed(2)}, {lookDir().y.toFixed(2)}, {lookDir().z.toFixed(2)}
                </span>
                <span class="text-emerald-400 border-l border-white/10 pl-2">
                    BOX: {geoBox().size.x.toFixed(0)}x{geoBox().size.y.toFixed(0)}x{geoBox().size.z.toFixed(0)} 
                </span>
            </div>
        </Show>
      </div>
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
