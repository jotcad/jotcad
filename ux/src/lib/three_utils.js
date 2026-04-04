import * as THREE from 'three';

let renderer;
const viewports = new Map(); // id -> { canvas, scene, camera }

export function initSharedRenderer(container) {
    if (renderer) return renderer;
    renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
    renderer.setPixelRatio(window.devicePixelRatio);
    renderer.setSize(window.innerWidth, window.innerHeight);
    renderer.setClearColor(0x000000, 0);
    return renderer;
}

export function createScene() {
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(45, 1, 0.1, 1000);
    camera.position.set(5, 5, 5);
    camera.lookAt(0, 0, 0);

    const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
    scene.add(ambientLight);

    const directionalLight = new THREE.DirectionalLight(0xffffff, 0.8);
    directionalLight.position.set(10, 10, 10);
    scene.add(directionalLight);

    return { scene, camera };
}

export function updateViewports() {
    if (!renderer) return;
    
    renderer.setClearColor(0x000000, 0);
    renderer.setScissorTest(false);
    renderer.clear();

    renderer.setScissorTest(true);

    viewports.forEach((vp, id) => {
        const rect = vp.canvas.getBoundingClientRect();
        if (rect.bottom < 0 || rect.top > renderer.domElement.clientHeight ||
            rect.right < 0 || rect.left > renderer.domElement.clientWidth) {
            return; // Off-screen
        }

        const width = rect.right - rect.left;
        const height = rect.bottom - rect.top;
        const left = rect.left;
        const bottom = renderer.domElement.clientHeight - rect.bottom;

        renderer.setViewport(left, bottom, width, height);
        renderer.setScissor(left, bottom, width, height);
        
        vp.camera.aspect = width / height;
        vp.camera.updateProjectionMatrix();
        
        renderer.render(vp.scene, vp.camera);
    });
}
