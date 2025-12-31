import * as THREE from 'three';
import { TrackballControls } from 'three/addons/controls/TrackballControls.js';
import { renderJotToThreejsScene } from 'jotcad-viewer';
import { GithubStorage } from './githubStorage.js';

const RELEASE_NUMBER = 1;

// Set Z-up as default for all Three.js objects
THREE.Object3D.DEFAULT_UP.set(0, 0, 1);

async function init() {
  const githubStorage = new GithubStorage();

  // 1. DOM References
  const viewportComponent = document.getElementById('viewport');
  const viewportContainer = viewportComponent.container;
  const orientationContainer = document.getElementById('orientationGuide');
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
  const showWorkbenchInput = document.getElementById('showWorkbench');

  const githubTokenInput = document.getElementById('githubToken');
  const githubRepoInput = document.getElementById('githubRepo');
  const githubPathInput = document.getElementById('githubPath');
  const githubLoadButton = document.getElementById('githubLoadButton');
  const githubSaveButton = document.getElementById('githubSaveButton');
  const githubTokenHelp = document.getElementById('githubTokenHelp');
  const helpWindow = document.getElementById('helpWindow');
  const toast = document.getElementById('toast');

  const showToast = (msg) => toast && toast.show(msg);

  // 2. Scene setup
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xcccccc);

  const furnitureGroup = new THREE.Group();
  scene.add(furnitureGroup);

  const createFurniture = () => {
    furnitureGroup.clear();
    const isVisible = localStorage.getItem('jotcad-showWorkbench') === 'true';
    if (isVisible) {
      const geometry = new THREE.BoxGeometry(200, 200, 20);
      geometry.translate(100, 100, -10); // Center at Z=-10 so top is Z=0
      const material = new THREE.MeshStandardMaterial({
        color: 0x888888,
        transparent: true,
        opacity: 0.3,
      });
      const workbench = new THREE.Mesh(geometry, material);
      furnitureGroup.add(workbench);

      // Add a grid on top of the workbench (Z=0)
      const grid = new THREE.GridHelper(200, 20, 0x444444, 0xaaaaaa);
      grid.rotation.x = Math.PI / 2;
      grid.position.set(100, 100, 0.05); // Just above Z=0 surface
      furnitureGroup.add(grid);

      // Add Ruler Markings and Labels
      const addRuler = (axis) => {
        for (let i = 0; i <= 200; i += 10) {
          // Tick mark
          const tickGeom = new THREE.BoxGeometry(
            axis === 'x' ? 1 : 2,
            axis === 'x' ? 2 : 1,
            0.1
          );
          const tick = new THREE.Mesh(
            tickGeom,
            new THREE.MeshBasicMaterial({ color: 0x000000 })
          );
          if (axis === 'x') tick.position.set(i, -2, 0.1);
          else tick.position.set(-2, i, 0.1);
          furnitureGroup.add(tick);

          // Number Label
          if (i % 20 === 0) {
            const label = makeTextSprite(i.toString(), 'black');
            label.scale.set(4, 4, 1);
            if (axis === 'x') label.position.set(i, -8, 0.1);
            else label.position.set(-8, i, 0.1);
            furnitureGroup.add(label);
          }
        }
        // Axis Title
        const title = makeTextSprite(axis.toUpperCase(), 'black');
        title.scale.set(8, 8, 1);
        if (axis === 'x') title.position.set(215, 0, 0.1);
        else title.position.set(0, 215, 0.1);
        furnitureGroup.add(title);
      };

      addRuler('x');
      addRuler('y');
    }
    if (showWorkbenchInput) {
      showWorkbenchInput.style.background = isVisible
        ? 'rgba(0, 255, 0, 0.2)'
        : 'rgba(255, 255, 255, 0.6)';
    }
  };

  showWorkbenchInput.addEventListener('click', () => {
    const isVisible = localStorage.getItem('jotcad-showWorkbench') === 'true';
    const nextState = !isVisible;
    localStorage.setItem('jotcad-showWorkbench', nextState);
    createFurniture();
  });

  const aspect =
    viewportContainer.clientWidth / (viewportContainer.clientHeight || 1);
  const frustumSize = 20;
  let camera = new THREE.OrthographicCamera(
    (-frustumSize * aspect) / 2,
    (frustumSize * aspect) / 2,
    frustumSize / 2,
    -frustumSize / 2,
    0.1, // Near plane small to avoid cutoff
    1000000
  );
  camera.position.set(-200, -200, 200); // Looking from -X, -Y, +Z
  camera.up.set(0, 0, 1);
  camera.lookAt(0, 0, 0);

  const renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(
    viewportContainer.clientWidth,
    viewportContainer.clientHeight
  );
  viewportContainer.appendChild(renderer.domElement);

  let controls = new TrackballControls(camera, renderer.domElement);
  controls.rotateSpeed = 1.0;
  controls.zoomSpeed = 1.2;
  controls.panSpeed = 0.8;
  controls.staticMoving = true;

  // Orientation Guide
  const axesScene = new THREE.Scene();
  const axesCamera = new THREE.PerspectiveCamera(50, 1, 0.1, 10);
  axesCamera.position.set(-2, -2, 2);
  axesCamera.up.set(0, 0, 1);
  axesCamera.lookAt(0, 0, 0);
  const axesRenderer = new THREE.WebGLRenderer({ alpha: true });
  axesRenderer.setSize(100, 100);
  orientationContainer.appendChild(axesRenderer.domElement);
  const axesHelper = new THREE.AxesHelper(1);
  axesScene.add(axesHelper);

  const makeTextSprite = (message, color) => {
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
  };

  const xLabel = makeTextSprite('X', 'red');
  xLabel.position.set(1.2, 0, 0);
  axesScene.add(xLabel);

  const yLabel = makeTextSprite('Y', 'green');
  yLabel.position.set(0, 1.2, 0);
  axesScene.add(yLabel);

  const zLabel = makeTextSprite('Z', 'blue');
  zLabel.position.set(0, 0, 1.2);
  axesScene.add(zLabel);

  // Lighting
  scene.add(new THREE.AmbientLight(0xffffff, 0.5));
  const dirLight = new THREE.DirectionalLight(0xffffff, 0.5);
  dirLight.position.set(0, 1, 1);
  scene.add(dirLight);

  // 3. Resizing
  const updateRendererSize = () => {
    const width = viewportContainer.clientWidth;
    const height = viewportContainer.clientHeight;
    if (width > 0 && height > 0) {
      renderer.setSize(width, height);
      const aspect = width / height;
      if (camera instanceof THREE.PerspectiveCamera) {
        camera.aspect = aspect;
      } else {
        const frustumHeight = camera.top - camera.bottom;
        camera.left = (-frustumHeight * aspect) / 2;
        camera.right = (frustumHeight * aspect) / 2;
      }
      camera.updateProjectionMatrix();
    }
  };

  const resizeObserver = new ResizeObserver(() => updateRendererSize());
  resizeObserver.observe(viewportContainer);
  window.addEventListener('resize', updateRendererSize);

  // 4. Logic & Events
  if (githubTokenHelp && helpWindow) {
    githubTokenHelp.addEventListener('click', () => {
      helpWindow.toggleMinimized();
    });
  }

  projectionTypeSelect.addEventListener('change', () => {
    const type = projectionTypeSelect.value;
    const oldCam = camera;
    const oldTarget = controls.target.clone();
    const aspect =
      viewportContainer.clientWidth / (viewportContainer.clientHeight || 1);
    const distance = oldCam.position.distanceTo(oldTarget);

    if (type === 'perspective') {
      const fov = parseFloat(fovInput.value);
      camera = new THREE.PerspectiveCamera(fov, aspect, 0.1, 1000000);
    } else {
      const fovRad = THREE.MathUtils.degToRad(
        oldCam.fov || parseFloat(fovInput.value)
      );
      const frustumHeight = 2 * distance * Math.tan(fovRad / 2);

      camera = new THREE.OrthographicCamera(
        (-frustumHeight * aspect) / 2,
        (frustumHeight * aspect) / 2,
        frustumHeight / 2,
        -frustumHeight / 2,
        0.1,
        1000000
      );
    }

    camera.position.copy(oldCam.position);
    camera.quaternion.copy(oldCam.quaternion);
    camera.up.set(0, 0, 1);
    camera.updateProjectionMatrix();

    controls.object = camera;
    controls.target.copy(oldTarget);
    controls.update();

    updateRendererSize();
  });

  githubLoadButton.addEventListener('click', async () => {
    const token = githubTokenInput.value;
    const repo = githubRepoInput.value;
    const fullPath = githubPathInput.value;
    const lastSlash = fullPath.lastIndexOf('/');
    const path = lastSlash !== -1 ? fullPath.substring(0, lastSlash) : '';
    const filename =
      lastSlash !== -1 ? fullPath.substring(lastSlash + 1) : fullPath;

    try {
      loadingIndicator.style.display = 'block';
      githubStorage.path = path;
      const content = await githubStorage.fetchFile(
        token,
        repo,
        path,
        filename
      );
      if (content) {
        codeEditor.value = content;
        localStorage.setItem('jotcad-codeEditor', content);
        showToast('Pulled from GitHub');
      } else {
        showToast('File not found');
      }
    } catch (e) {
      showToast('Pull failed: ' + e.message);
    } finally {
      loadingIndicator.style.display = 'none';
    }
  });

  githubSaveButton.addEventListener('click', async () => {
    const token = githubTokenInput.value;
    const repo = githubRepoInput.value;
    const fullPath = githubPathInput.value;
    const lastSlash = fullPath.lastIndexOf('/');
    const path = lastSlash !== -1 ? fullPath.substring(0, lastSlash) : '';
    const filename =
      lastSlash !== -1 ? fullPath.substring(lastSlash + 1) : fullPath;

    try {
      loadingIndicator.style.display = 'block';
      await githubStorage.saveFile(
        token,
        repo,
        path,
        filename,
        codeEditor.value
      );
      showToast('Committed to GitHub');
    } catch (e) {
      showToast('Commit failed: ' + e.message);
    } finally {
      loadingIndicator.style.display = 'none';
    }
  });

  const clearScene = () => {
    while (scene.children.length > 0) {
      const obj = scene.children[0];
      if (obj === furnitureGroup) {
        scene.remove(obj);
        continue;
      }
      if (obj.geometry) obj.geometry.dispose();
      if (obj.material)
        Array.isArray(obj.material)
          ? obj.material.forEach((m) => m.dispose())
          : obj.material.dispose();
      scene.remove(obj);
    }
    scene.add(furnitureGroup);
    scene.add(new THREE.AmbientLight(0xffffff, 0.5));
    scene.add(dirLight);
    scene.add(camera);
  };

  let currentJotText = '';
  const renderJotString = async (text) => {
    currentJotText = text;
    clearScene();
    try {
      await renderJotToThreejsScene(text, scene, {
        edgeThresholdAngle: parseFloat(edgeThresholdInput.value),
      });
      const box = new THREE.Box3().setFromObject(scene);
      const center = box.getCenter(new THREE.Vector3());
      const size = box.getSize(new THREE.Vector3());
      const maxSize = Math.max(size.x, size.y, size.z);
      const distance = maxSize * 2 || 20;
      const direction = new THREE.Vector3()
        .subVectors(camera.position, controls.target)
        .normalize();
      camera.position.copy(center).add(direction.multiplyScalar(distance));
      controls.target.copy(center);
      controls.update();
    } catch (e) {
      console.error(e);
    }
  };

  renderButton.addEventListener('click', async () => {
    loadingIndicator.style.display = 'block';
    try {
      const code = `${codeEditor.value.trim()}.jot('${
        outputFilenameInput.value
      }')`;
      const res = await fetch(
        `${serverAddressInput.value}/run/${sessionIdInput.value}/${outputFilenameInput.value}`,
        {
          method: 'POST',
          body: code,
        }
      );
      if (res.ok) await renderJotString(await res.text());
    } finally {
      loadingIndicator.style.display = 'none';
    }
  });

  // Init from storage
  serverAddressInput.value =
    localStorage.getItem('jotcad-serverAddress') || 'http://localhost:3000';
  sessionIdInput.value =
    localStorage.getItem('jotcad-sessionId') || 'defaultSession';
  outputFilenameInput.value =
    localStorage.getItem('jotcad-outputFilename') || 'out.jot';
  codeEditor.value = localStorage.getItem('jotcad-codeEditor') || 'Box3(10)';
  githubTokenInput.value = localStorage.getItem('jotcad-githubToken') || '';
  githubRepoInput.value = localStorage.getItem('jotcad-githubRepo') || '';
  githubPathInput.value =
    localStorage.getItem('jotcad-githubPath') || 'designs/cup.js';
  createFurniture();
  releaseNumberSpan.textContent = `Release: ${RELEASE_NUMBER}`;

  const savedProjectionType =
    localStorage.getItem('jotcad-projectionType') || 'orthographic';
  projectionTypeSelect.value = savedProjectionType;
  // Trigger projection change logic but handle the initial state where oldCam orientation might be default
  const initProjection = () => {
    const type = projectionTypeSelect.value;
    const aspect =
      viewportContainer.clientWidth / (viewportContainer.clientHeight || 1);

    if (type === 'perspective') {
      const fov = parseFloat(fovInput.value);
      camera = new THREE.PerspectiveCamera(fov, aspect, 0.1, 1000000);
    } else {
      const distance = 350; // Use a fixed distance for initial ortho height calculation
      const fovRad = THREE.MathUtils.degToRad(parseFloat(fovInput.value));
      const frustumHeight = 2 * distance * Math.tan(fovRad / 2);

      camera = new THREE.OrthographicCamera(
        (-frustumHeight * aspect) / 2,
        (frustumHeight * aspect) / 2,
        frustumHeight / 2,
        -frustumHeight / 2,
        0.1,
        1000000
      );
    }
    camera.position.set(-200, -200, 200);
    camera.up.set(0, 0, 1);
    camera.lookAt(0, 0, 0);
    camera.updateProjectionMatrix();
    controls.object = camera;
    controls.update();
  };
  initProjection();

  const animate = () => {
    requestAnimationFrame(animate);

    controls.update();

    const dir = new THREE.Vector3()

      .subVectors(camera.position, controls.target)

      .normalize();

    axesCamera.position.copy(dir).multiplyScalar(3);

    axesCamera.up.copy(camera.up);

    axesCamera.lookAt(0, 0, 0);

    renderer.render(scene, camera);

    axesRenderer.render(axesScene, axesCamera);
  };
  animate();

  // Settings syncing
  const syncInputs = [
    rotateSpeedInput,
    zoomSpeedInput,
    panSpeedInput,
    fovInput,
    edgeThresholdInput,
  ];
  syncInputs.forEach((el) => {
    if (!el) return;
    const savedVal = localStorage.getItem('jotcad-' + el.id);
    if (savedVal !== null) {
      el.value = savedVal;
      const display = document.getElementById(el.id + 'Value');
      if (display) display.textContent = parseFloat(savedVal).toFixed(1);
    }

    el.addEventListener('input', () => {
      const val = parseFloat(el.value);
      const display = document.getElementById(el.id + 'Value');
      if (display) display.textContent = val.toFixed(1);
      localStorage.setItem('jotcad-' + el.id, val);
      if (el.id === 'rotateSpeed') controls.rotateSpeed = val;
      if (el.id === 'zoomSpeed') controls.zoomSpeed = val;
      if (el.id === 'panSpeed') controls.panSpeed = val;
      if (el.id === 'fov' && camera instanceof THREE.PerspectiveCamera) {
        camera.fov = val;
        camera.updateProjectionMatrix();
      }
      if (el.id === 'edgeThreshold' && currentJotText)
        renderJotString(currentJotText);
    });
  });

  [
    serverAddressInput,
    sessionIdInput,
    outputFilenameInput,
    codeEditor,
    githubTokenInput,
    githubRepoInput,
    githubPathInput,
  ].forEach((el) => {
    el.addEventListener('input', () => {
      localStorage.setItem('jotcad-' + el.id, el.value);
    });
  });

  codeEditor.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.key === 'Enter') {
      e.preventDefault();
      renderButton.click();
    }
  });
}

init();
