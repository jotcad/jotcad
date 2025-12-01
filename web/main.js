import * as THREE from 'three';
import { OrbitControls } from 'three/addons/controls/OrbitControls.js';
import { renderJotToThreejsScene } from 'jotcad-viewer';

// Manually managed release number for version confirmation
const RELEASE_NUMBER = 1;

async function init() {
  // 1. Scene setup
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xcccccc);

  // 2. Camera setup
  const camera = new THREE.PerspectiveCamera(
    75,
    window.innerWidth / window.innerHeight,
    0.1,
    1000
  );
  camera.position.z = 5;

  // 3. Renderer setup
  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(window.innerWidth, window.innerHeight);
  document.body.appendChild(renderer.domElement);

  // 4. Controls setup
  const controls = new OrbitControls(camera, renderer.domElement);
  controls.enableDamping = true; // an animation loop is required when damping is enabled
  controls.dampingFactor = 0.25;
  controls.screenSpacePanning = false;
  controls.minDistance = 1;
  controls.maxDistance = 100;
  controls.maxPolarAngle = Math.PI / 2;

  // 5. Lighting
  const ambientLight = new THREE.AmbientLight(0xffffff, 0.5);
  scene.add(ambientLight);
  const directionalLight = new THREE.DirectionalLight(0xffffff, 0.5);
  directionalLight.position.set(0, 1, 1);
  scene.add(directionalLight);

  // Get references to UI elements
  const codeEditor = document.getElementById('codeEditor');
  const renderButton = document.getElementById('renderButton');
  const serverAddressInput = document.getElementById('serverAddress');
  const sessionIdInput = document.getElementById('sessionId');
  const outputFilenameInput = document.getElementById('outputFilename');
  const loadingIndicator = document.getElementById('loadingIndicator');
  const releaseNumberSpan = document.getElementById('releaseNumber');
  let loadingCount = 0;

  // Display the release number
  releaseNumberSpan.textContent = `Release: ${RELEASE_NUMBER}`;

  serverAddressInput.value =
    localStorage.getItem('jotcad-serverAddress') || 'http://localhost:8080';
  sessionIdInput.value = localStorage.getItem('jotcad-sessionId') || '';
  outputFilenameInput.value =
    localStorage.getItem('jotcad-outputFilename') || 'out.jot';

  const defaultCode = `Box3(10)`;
  codeEditor.value = localStorage.getItem('jotcad-codeEditor') || defaultCode;

  // Function to clear the scene
  function clearScene() {
    while (scene.children.length > 0) {
      const object = scene.children[0];
      if (
        object instanceof THREE.Mesh ||
        object instanceof THREE.LineSegments
      ) {
        object.geometry.dispose();
        object.material.dispose();
      }
      scene.remove(object);
    }
    scene.add(ambientLight);
    scene.add(directionalLight);
    scene.add(camera);
  }

  // Function to render a JOT string
  async function renderJotString(jotText) {
    clearScene(); // Clear previous model
    try {
      await renderJotToThreejsScene(jotText, scene);
      console.log('JOT data rendered to scene.');

      // Frame the whole shape
      const box = new THREE.Box3().setFromObject(scene);
      const size = box.getSize(new THREE.Vector3());
      const center = box.getCenter(new THREE.Vector3());
      const maxSize = Math.max(size.x, size.y, size.z);
      const fitHeightDistance =
        maxSize / (2 * Math.tan((Math.PI * camera.fov) / 360));
      const fitWidthDistance = fitHeightDistance / camera.aspect;
      const distance = 2.0 * Math.max(fitHeightDistance, fitWidthDistance);

      const direction = new THREE.Vector3()
        .subVectors(camera.position, controls.target)
        .normalize();

      camera.position
        .copy(center)
        .add(direction.clone().multiplyScalar(distance));
      controls.target.copy(center);
      controls.update();
    } catch (error) {
      console.error('Failed to render JOT data:', error, 'JOT text:', jotText);
      // Fallback: add a dummy cube if JOT fails
      const geometry = new THREE.BoxGeometry(1, 1, 1);
      const material = new THREE.MeshStandardMaterial({ color: 0xff0000 });
      const cube = new THREE.Mesh(geometry, material);
      scene.add(cube);
    }
  }

  // Event listener for the render button
  renderButton.addEventListener('click', async () => {
    let code = codeEditor.value.trim(); // Trim whitespace
    const serverAddress = serverAddressInput.value;
    const sessionId = sessionIdInput.value;
    const outputFilename = outputFilenameInput.value;

    // Make the .jot() call implicit (unconditionally append)
    code = `${code}.jot('${outputFilename}')`;

    console.log('Sending code to server for rendering...');
    loadingCount++;
    loadingIndicator.style.display = 'block';
    try {
      const response = await fetch(
        `${serverAddress}/run/${sessionId}/${outputFilename}`,
        {
          method: 'POST',
          headers: {
            'Content-Type': 'text/plain',
          },
          body: code,
        }
      );

      if (!response.ok) {
        const errorText = await response.text();
        throw new Error(
          `HTTP error! status: ${response.status}, message: ${errorText}`
        );
      }

      const jotText = await response.text();
      console.log('Received JOT data from server.');
      await renderJotString(jotText);
    } catch (error) {
      console.error('Failed to send code or receive JOT data:', error);
      clearScene();
      const geometry = new THREE.BoxGeometry(1, 1, 1);
      const material = new THREE.MeshStandardMaterial({ color: 0xff0000 });
      const errorCube = new THREE.Mesh(geometry, material);
      scene.add(errorCube);
    } finally {
      loadingCount--;
      if (loadingCount === 0) {
        loadingIndicator.style.display = 'none';
      }
    }
  });

  codeEditor.addEventListener('keydown', (e) => {
    if (e.shiftKey && e.key === 'Enter') {
      e.preventDefault();
    }
  });

  codeEditor.addEventListener('input', (event) => {
    localStorage.setItem('jotcad-codeEditor', event.target.value);
  });

  // 8. Animation loop
  function animate() {
    requestAnimationFrame(animate);
    controls.update();
    renderer.render(scene, camera);
  }
  animate();

  window.addEventListener('resize', () => {
    camera.aspect = window.innerWidth / window.innerHeight;
    camera.updateProjectionMatrix();
    renderer.setSize(window.innerWidth, window.innerHeight);
  });

  serverAddressInput.addEventListener('input', (event) => {
    localStorage.setItem('jotcad-serverAddress', event.target.value);
  });
  sessionIdInput.addEventListener('input', (event) => {
    localStorage.setItem('jotcad-sessionId', event.target.value);
  });
  outputFilenameInput.addEventListener('input', (event) => {
    localStorage.setItem('jotcad-outputFilename', event.target.value);
  });
}

init();
