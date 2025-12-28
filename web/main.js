import * as THREE from 'three';
import { TrackballControls } from 'three/addons/controls/TrackballControls.js';
import { renderJotToThreejsScene } from 'jotcad-viewer';

// Manually managed release number for version confirmation
const RELEASE_NUMBER = 1;

async function init() {
  // 1. Scene setup
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xcccccc);

  // 2. Camera setup
  const aspect = window.innerWidth / window.innerHeight;
  const frustumSize = 20;
  let camera = new THREE.OrthographicCamera(
    (-frustumSize * aspect) / 2,
    (frustumSize * aspect) / 2,
    frustumSize / 2,
    -frustumSize / 2,
    0.1,
    1000
  );
  camera.position.set(0, 0, 20);

  // 3. Renderer setup
  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(window.innerWidth, window.innerHeight);
  document.body.appendChild(renderer.domElement);

  // 4. Controls setup
  let controls = new TrackballControls(camera, renderer.domElement);
  controls.rotateSpeed = 1.0;
  controls.zoomSpeed = 1.2;
  controls.panSpeed = 0.8;
  controls.staticMoving = true;

  // 5. Orientation Guide (Secondary Scene)
  const orientationContainer = document.getElementById('orientationGuide');
  const axesScene = new THREE.Scene();
  const axesCamera = new THREE.PerspectiveCamera(50, 1, 0.1, 10);
  axesCamera.position.set(0, 0, 3);
  axesCamera.lookAt(0, 0, 0);
  const axesRenderer = new THREE.WebGLRenderer({ alpha: true });
  axesRenderer.setSize(100, 100);
  orientationContainer.appendChild(axesRenderer.domElement);

  const axesHelper = new THREE.AxesHelper(1);
  axesScene.add(axesHelper);

  // Add labels to axesScene
  function makeTextSprite(message, color) {
    const canvas = document.createElement('canvas');
    const context = canvas.getContext('2d');
    canvas.width = 64;
    canvas.height = 64;
    context.font = 'bold 48px Arial';
    context.fillStyle = color;
    context.textAlign = 'center';
    context.textBaseline = 'middle';
    context.fillText(message, 32, 32);

    const texture = new THREE.CanvasTexture(canvas);
    const spriteMaterial = new THREE.SpriteMaterial({ map: texture });
    const sprite = new THREE.Sprite(spriteMaterial);
    sprite.scale.set(0.5, 0.5, 1);
    return sprite;
  }

  const xLabel = makeTextSprite('X', 'red');
  xLabel.position.set(1.2, 0, 0);
  axesScene.add(xLabel);

  const yLabel = makeTextSprite('Y', 'green');
  yLabel.position.set(0, 1.2, 0);
  axesScene.add(yLabel);

  const zLabel = makeTextSprite('Z', 'blue');
  zLabel.position.set(0, 0, 1.2);
  axesScene.add(zLabel);

  // 6. Lighting
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
  const rotateSpeedInput = document.getElementById('rotateSpeed');
  const rotateSpeedValue = document.getElementById('rotateSpeedValue');
  const zoomSpeedInput = document.getElementById('zoomSpeed');
  const zoomSpeedValue = document.getElementById('zoomSpeedValue');
  const panSpeedInput = document.getElementById('panSpeed');
  const panSpeedValue = document.getElementById('panSpeedValue');
  const fovInput = document.getElementById('fov');
  const fovValue = document.getElementById('fovValue');
  const edgeThresholdInput = document.getElementById('edgeThreshold');
  const edgeThresholdValue = document.getElementById('edgeThresholdValue');
  const projectionTypeSelect = document.getElementById('projectionType');
  const loadingIndicator = document.getElementById('loadingIndicator');
  const releaseNumberSpan = document.getElementById('releaseNumber');
  const fileList = document.getElementById('fileList');
  let loadingCount = 0;
  let currentJotText = '';

  // Display the release number
  releaseNumberSpan.textContent = `Release: ${RELEASE_NUMBER}`;

  serverAddressInput.value =
    localStorage.getItem('jotcad-serverAddress') || 'http://localhost:3000';
  sessionIdInput.value =
    localStorage.getItem('jotcad-sessionId') || 'defaultSession';
  outputFilenameInput.value =
    localStorage.getItem('jotcad-outputFilename') || 'out.jot';

  const defaultCode = `Box3(10)`;
  codeEditor.value = localStorage.getItem('jotcad-codeEditor') || defaultCode;

  // Function to update the file list
  async function updateFileList() {
    const serverAddress = serverAddressInput.value;
    const sessionId = sessionIdInput.value;
    try {
      const response = await fetch(`${serverAddress}/list/${sessionId}`);
      if (response.ok) {
        const files = await response.json();
        fileList.innerHTML = '';
        files.forEach((file) => {
          const li = document.createElement('li');
          const a = document.createElement('a');
          a.href = `${serverAddress}/get/${sessionId}/${file}`;
          a.textContent = file;
          a.download = file;
          a.style.color = 'blue';
          a.style.textDecoration = 'underline';
          a.style.cursor = 'pointer';
          li.appendChild(a);
          fileList.appendChild(li);
        });
      }
    } catch (error) {
      console.error('Failed to fetch file list:', error);
    }
  }

  // Function to clear the scene
  function clearScene() {
    while (scene.children.length > 0) {
      const object = scene.children[0];
      if (
        object instanceof THREE.Mesh ||
        object instanceof THREE.LineSegments ||
        object instanceof THREE.Group ||
        object instanceof THREE.Object3D
      ) {
        if (object.geometry) object.geometry.dispose();
        if (object.material) {
          if (Array.isArray(object.material)) {
            object.material.forEach((m) => m.dispose());
          } else {
            object.material.dispose();
          }
        }
      }
      scene.remove(object);
    }
    scene.add(ambientLight);
    scene.add(directionalLight);
    scene.add(camera);
  }

  // Function to render a JOT string
  async function renderJotString(jotText) {
    currentJotText = jotText;
    clearScene(); // Clear previous model
    try {
      const edgeThresholdAngle = parseFloat(edgeThresholdInput.value);
      await renderJotToThreejsScene(jotText, scene, { edgeThresholdAngle });
      console.log('JOT data rendered to scene.');

      // Frame the whole shape
      const box = new THREE.Box3().setFromObject(scene);
      const size = box.getSize(new THREE.Vector3());
      const center = box.getCenter(new THREE.Vector3());
      const maxSize = Math.max(size.x, size.y, size.z);

      if (camera instanceof THREE.PerspectiveCamera) {
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
      } else if (camera instanceof THREE.OrthographicCamera) {
        const aspect = window.innerWidth / window.innerHeight;
        const margin = 1.2;
        if (size.x / aspect > size.y) {
          camera.top = (margin * size.x) / aspect / 2;
          camera.bottom = (-margin * size.x) / aspect / 2;
          camera.left = (-margin * size.x) / 2;
          camera.right = (margin * size.x) / 2;
        } else {
          camera.top = (margin * size.y) / 2;
          camera.bottom = (-margin * size.y) / 2;
          camera.left = (-margin * size.y * aspect) / 2;
          camera.right = (margin * size.y * aspect) / 2;
        }
        camera.updateProjectionMatrix();
        const distance = maxSize * 2;
        const direction = new THREE.Vector3()
          .subVectors(camera.position, controls.target)
          .normalize();
        camera.position
          .copy(center)
          .add(direction.clone().multiplyScalar(distance));
      }

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
      console.log(jotText); // Log the actual JOT text
      await renderJotString(jotText);
      await updateFileList();
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
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      renderButton.click();
    }
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

    // Sync orientation guide camera
    // We want the axesCamera to look at (0,0,0) from the same relative direction
    // as the main camera looks at its target.
    const dir = new THREE.Vector3()
      .subVectors(camera.position, controls.target)
      .normalize();
    axesCamera.position.copy(dir).multiplyScalar(3);
    axesCamera.lookAt(0, 0, 0);

    renderer.render(scene, camera);
    axesRenderer.render(axesScene, axesCamera);
  }
  animate();

  window.addEventListener('resize', () => {
    const width = window.innerWidth;
    const height = window.innerHeight;
    const aspect = width / height;

    if (camera instanceof THREE.PerspectiveCamera) {
      camera.aspect = aspect;
    } else if (camera instanceof THREE.OrthographicCamera) {
      const frustumSize = camera.top - camera.bottom;
      camera.left = (-frustumSize * aspect) / 2;
      camera.right = (frustumSize * aspect) / 2;
    }

    camera.updateProjectionMatrix();
    renderer.setSize(width, height);
    controls.handleResize();
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

  rotateSpeedInput.addEventListener('input', (event) => {
    const speed = parseFloat(event.target.value);
    controls.rotateSpeed = speed;
    rotateSpeedValue.textContent = speed.toFixed(1);
    localStorage.setItem('jotcad-rotateSpeed', speed);
  });

  zoomSpeedInput.addEventListener('input', (event) => {
    const speed = parseFloat(event.target.value);
    controls.zoomSpeed = speed;
    zoomSpeedValue.textContent = speed.toFixed(1);
    localStorage.setItem('jotcad-zoomSpeed', speed);
  });

  panSpeedInput.addEventListener('input', (event) => {
    const speed = parseFloat(event.target.value);
    controls.panSpeed = speed;
    panSpeedValue.textContent = speed.toFixed(1);
    localStorage.setItem('jotcad-panSpeed', speed);
  });

  fovInput.addEventListener('input', (event) => {
    const value = parseFloat(event.target.value);
    fovValue.textContent = value;
    localStorage.setItem('jotcad-fov', value);
    if (camera instanceof THREE.PerspectiveCamera) {
      camera.fov = value;
      camera.updateProjectionMatrix();
    }
  });

  edgeThresholdInput.addEventListener('input', (event) => {
    const value = event.target.value;
    edgeThresholdValue.textContent = value;
    localStorage.setItem('jotcad-edgeThreshold', value);
    if (currentJotText) {
      renderJotString(currentJotText);
    }
  });

  projectionTypeSelect.addEventListener('change', (event) => {
    const type = event.target.value;
    localStorage.setItem('jotcad-projectionType', type);

    const oldCamera = camera;
    const aspect = window.innerWidth / window.innerHeight;

    if (type === 'perspective') {
      const fov = parseFloat(fovInput.value);
      camera = new THREE.PerspectiveCamera(fov, aspect, 0.1, 1000);
    } else {
      const frustumSize = 20;
      camera = new THREE.OrthographicCamera(
        (-frustumSize * aspect) / 2,
        (frustumSize * aspect) / 2,
        frustumSize / 2,
        -frustumSize / 2,
        0.1,
        1000
      );
    }

    camera.position.copy(oldCamera.position);
    camera.quaternion.copy(oldCamera.quaternion);
    camera.updateProjectionMatrix();

    controls.dispose();
    controls = new TrackballControls(camera, renderer.domElement);
    controls.rotateSpeed = parseFloat(rotateSpeedInput.value);
    controls.zoomSpeed = parseFloat(zoomSpeedInput.value);
    controls.panSpeed = parseFloat(panSpeedInput.value);
    controls.staticMoving = true;

    if (currentJotText) {
      renderJotString(currentJotText);
    }
  });

  // Initialize from localStorage
  const savedRotateSpeed = localStorage.getItem('jotcad-rotateSpeed');
  if (savedRotateSpeed) {
    const speed = parseFloat(savedRotateSpeed);
    controls.rotateSpeed = speed;
    rotateSpeedInput.value = speed;
    rotateSpeedValue.textContent = speed.toFixed(1);
  }

  const savedZoomSpeed = localStorage.getItem('jotcad-zoomSpeed');
  if (savedZoomSpeed) {
    const speed = parseFloat(savedZoomSpeed);
    controls.zoomSpeed = speed;
    zoomSpeedInput.value = speed;
    zoomSpeedValue.textContent = speed.toFixed(1);
  }

  const savedPanSpeed = localStorage.getItem('jotcad-panSpeed');
  if (savedPanSpeed) {
    const speed = parseFloat(savedPanSpeed);
    controls.panSpeed = speed;
    panSpeedInput.value = speed;
    panSpeedValue.textContent = speed.toFixed(1);
  }

  const savedFov = localStorage.getItem('jotcad-fov');
  if (savedFov) {
    fovInput.value = savedFov;
    fovValue.textContent = savedFov;
  }

  const savedEdgeThreshold = localStorage.getItem('jotcad-edgeThreshold');
  if (savedEdgeThreshold) {
    edgeThresholdInput.value = savedEdgeThreshold;
    edgeThresholdValue.textContent = savedEdgeThreshold;
  }

  const savedProjectionType =
    localStorage.getItem('jotcad-projectionType') || 'orthographic';
  projectionTypeSelect.value = savedProjectionType;
  projectionTypeSelect.dispatchEvent(new Event('change'));
}

init();
