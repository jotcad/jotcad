import { onMount, onCleanup, createEffect } from 'solid-js';
import * as THREE from 'three';
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls';
import { renderJotToScene } from '../lib/three_utils';
import { vfs } from '../lib/blackboard';

export const Viewport = (props) => {
    let containerRef;
    let renderer, scene, camera, controls;
    let animationId;

    const init = () => {
        if (!containerRef) return;

        const width = containerRef.clientWidth;
        const height = containerRef.clientHeight;

        scene = new THREE.Scene();
        scene.background = new THREE.Color(0x1e293b); // Slate-800

        camera = new THREE.PerspectiveCamera(45, width / height, 0.1, 10000);
        camera.position.set(0, 0, 1);
        camera.up.set(0, 1, 0);
        camera.lookAt(0, 0, 0);

        renderer = new THREE.WebGLRenderer({ antialias: true });
        renderer.setSize(width, height);
        renderer.setPixelRatio(window.devicePixelRatio);
        containerRef.appendChild(renderer.domElement);

        controls = new OrbitControls(camera, renderer.domElement);
        controls.enableDamping = true;

        // Lights
        scene.add(new THREE.AmbientLight(0xffffff, 0.6));
        const dl = new THREE.DirectionalLight(0xffffff, 0.8);
        dl.position.set(100, 200, 100);
        scene.add(dl);

        const animate = () => {
            animationId = requestAnimationFrame(animate);
            controls.update();
            renderer.render(scene, camera);
        };
        animate();

        // Handle resizing
        const resizeObserver = new ResizeObserver(() => {
            const w = containerRef.clientWidth;
            const h = containerRef.clientHeight;
            renderer.setSize(w, h);
            camera.aspect = w / h;
            camera.updateProjectionMatrix();
        });
        resizeObserver.observe(containerRef);

        onCleanup(() => {
            cancelAnimationFrame(animationId);
            resizeObserver.disconnect();
            renderer.dispose();
            if (containerRef && renderer.domElement) {
                containerRef.removeChild(renderer.domElement);
            }
        });
    };

    onMount(() => {
        init();
    });

    createEffect(async () => {
        const data = props.data;
        if (data && scene) {
            // Clear previous geometry
            while(scene.children.length > 0){ 
                const obj = scene.children[0];
                if (obj.type === 'Mesh') {
                    obj.geometry.dispose();
                    obj.material.dispose();
                }
                scene.remove(obj); 
            }
            
            // Re-add lights
            scene.add(new THREE.AmbientLight(0xffffff, 0.6));
            const dl = new THREE.DirectionalLight(0xffffff, 0.8);
            dl.position.set(100, 200, 100);
            scene.add(dl);

            try {
                await renderJotToScene(vfs, data, scene);
                
                // Auto-frame geometry
                const box = new THREE.Box3().setFromObject(scene);
                const center = box.getCenter(new THREE.Vector3());
                const size = box.getSize(new THREE.Vector3());
                const maxDim = Math.max(size.x, size.y, size.z) || 1;
                
                // Maintain the 0,0,1 direction but move back to frame the object
                const distance = maxDim * 2;
                camera.position.set(center.x, center.y, center.z + distance);
                camera.lookAt(center);
                camera.up.set(0, 1, 0);
                
                controls.target.copy(center);
                controls.update();
            } catch (e) {
                console.warn('[Viewport] Failed to render data:', e);
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
