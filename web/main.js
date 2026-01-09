import * as THREE from 'three';
import { TrackballControls } from 'three/addons/controls/TrackballControls.js';
import { renderJotToThreejsScene } from 'jotcad-viewer';
import { GithubStorage } from './githubStorage.js';
import { DesignStorage } from './designStorage.js';

const RELEASE_NUMBER = 1;

const EDIT_ICON = `
<svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="vertical-align: middle; margin-right: 4px; flex-shrink: 0;">
  <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path>
  <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path>
</svg>`;

// Set Z-up as default for all Three.js objects
THREE.Object3D.DEFAULT_UP.set(0, 0, 1);

async function init() {
  const githubStorage = new GithubStorage();
  const designStorage = new DesignStorage();
  await designStorage.init();

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
  const fetchDesignsButton = document.getElementById('fetchDesignsButton');
  const githubFileList = document.getElementById('githubFileList');
  const githubTokenHelp = document.getElementById('githubTokenHelp');
  const helpWindow = document.getElementById('helpWindow');
  const editorWindow = document.getElementById('editorWindow');
  const toast = document.getElementById('toast');
  const uiScaleToggle = document.getElementById('uiScaleToggle');

  const showToast = (msg) => toast && toast.show(msg);
  const showAlert = (msg) => window.alert(msg);

  const readEditor = () => codeEditor.value;
  const writeEditor = async (content, updateMirror = true) => {
    codeEditor.value = content;
    localStorage.setItem('jotcad-codeEditor', content);
    if (updateMirror) {
      const fullPath = githubPathInput.value.trim();
      await designStorage.saveLocal(fullPath, content);
      await updateUnpushedIndicator();
    }
  };

  const prettifyCode = async () => {
    if (
      typeof prettier === 'undefined' ||
      typeof prettierPlugins === 'undefined'
    ) {
      console.warn('Prettier not loaded yet');
      return;
    }
    try {
      const content = readEditor();
      const formatted = prettier.format(content, {
        parser: 'babel',
        plugins: prettierPlugins,
        singleQuote: true,
        trailingComma: 'es5',
        semi: false,
        printWidth: 40,
      });
      if (formatted !== content) {
        await writeEditor(formatted);
      }
    } catch (e) {
      console.warn('Prettify failed:', e);
    }
  };

  const reportError = (prefix, e) => {
    let message = e.message;
    if (e.status === 401) {
      message =
        'GitHub Token is invalid or has expired. Please check your settings.';
    } else if (e.status === 404) {
      message = 'GitHub Repository or Path not found.';
    } else if (e.status === 403) {
      message = 'GitHub API rate limit exceeded or access forbidden.';
    }
    console.error(`${prefix}:`, e);
    showAlert(`${prefix}: ${message}`);
  };

  const fetchGithubDesigns = async () => {
    console.log('fetchGithubDesigns: Starting...');
    if (!githubFileList) {
      console.error('fetchGithubDesigns: githubFileList element not found');
      return;
    }
    const token = githubTokenInput.value.trim();
    const repo = githubRepoInput.value.trim();
    const fullPath = githubPathInput.value.trim();
    const lastSlash = fullPath.lastIndexOf('/');
    const path = lastSlash !== -1 ? fullPath.substring(0, lastSlash) : '';

    console.log('fetchGithubDesigns: Params', { repo, path });

    if (!token || !repo) {
      showAlert('GitHub Token and Repo required in Config window.');
      return;
    }

    try {
      loadingIndicator.style.display = 'block';
      console.log('fetchGithubDesigns: Calling listFiles...');
      const remoteFiles = await githubStorage.listFiles(token, repo, path);

      // Get all local designs from indexedDB
      const allLocalDesigns = await designStorage.getAllDesigns();
      // Filter for those matching current repo path prefix if applicable,
      // but for now let's just use the current path logic.
      const localPathsInCurrentDir = allLocalDesigns
        .filter((d) => {
          const dPath =
            d.fullPath.lastIndexOf('/') !== -1
              ? d.fullPath.substring(0, d.fullPath.lastIndexOf('/'))
              : '';
          return dPath === path;
        })
        .map((d) => d.fullPath.substring(d.fullPath.lastIndexOf('/') + 1));

      // Merge unique filenames
      const allFilenames = Array.from(
        new Set([...remoteFiles, ...localPathsInCurrentDir])
      );
      allFilenames.sort();

      githubFileList.innerHTML = '';
      for (const file of allFilenames) {
        const fullFilePath = path ? `${path}/${file}` : file;
        const hasChanges = await designStorage.hasUnpushedChanges(fullFilePath);

        const btn = document.createElement('button');
        btn.innerHTML =
          (hasChanges ? EDIT_ICON : '') + `<span>${fullFilePath}</span>`;
        btn.style.textAlign = 'left';
        btn.style.fontSize = '11px';
        btn.style.display = 'flex';
        btn.style.alignItems = 'center';
        if (hasChanges) btn.style.color = '#ffaa00';

        btn.onclick = async () => {
          githubPathInput.value = fullFilePath;
          localStorage.setItem('jotcad-githubPath', fullFilePath);
          await loadDesign(fullFilePath);
        };
        githubFileList.appendChild(btn);
      }
      showToast(`Fetched ${allFilenames.length} designs`);
    } catch (e) {
      reportError('Fetch Designs failed', e);
    } finally {
      loadingIndicator.style.display = 'none';
    }
  };

  const updateUnpushedIndicator = async () => {
    const fullPath = githubPathInput.value;
    const hasChanges = await designStorage.hasUnpushedChanges(fullPath);
    if (githubSaveButton) {
      githubSaveButton.style.border = hasChanges ? '2px solid #ffaa00' : '';
      githubSaveButton.style.boxShadow = hasChanges ? '0 0 5px #ffaa00' : '';
    }
  };

  const loadDesign = async (fullPath) => {
    if (!fullPath) return;
    const design = await designStorage.getDesign(fullPath);
    if (design && design.local !== undefined) {
      await writeEditor(design.local, false);
    } else {
      // If we don't have a local mirror, trigger a pull from GitHub
      githubLoadButton.click();
    }

    // Ensure the editor window is visible when loading a design
    if (editorWindow && editorWindow.hasAttribute('minimized')) {
      editorWindow.removeAttribute('minimized');
    }

    await updateUnpushedIndicator();
  };

  // UI Scale Logic
  const uiScales = [1.0, 0.8, 0.6, 0.5, 0.4, 0.25, 0.2, 1.25, 1.5];
  let currentScaleIndex = 0;

  const setScale = (scale) => {
    document.documentElement.style.setProperty('--ui-scale', scale);
    localStorage.setItem('jotcad-uiScale', scale);
    if (uiScaleToggle) uiScaleToggle.textContent = scale + 'x';
  };

  if (uiScaleToggle) {
    uiScaleToggle.textContent = uiScales[currentScaleIndex] + 'x';
    const savedScale = parseFloat(localStorage.getItem('jotcad-uiScale'));
    if (!isNaN(savedScale)) {
      // Find closest index or just use it
      const foundIndex = uiScales.indexOf(savedScale);
      if (foundIndex !== -1) {
        currentScaleIndex = foundIndex;
      } else {
        // If saved scale isn't in our list, just append it or set custom
        setScale(savedScale);
      }
      setScale(savedScale);
    }

    uiScaleToggle.addEventListener('click', () => {
      currentScaleIndex = (currentScaleIndex + 1) % uiScales.length;
      const newScale = uiScales[currentScaleIndex];
      setScale(newScale);
      showToast(`UI Scale: ${newScale}x`);
    });
  }

  // Console Redirection
  const consoleOutput = document.getElementById('consoleOutput');
  const originalConsole = {
    log: console.log,
    error: console.error,
    warn: console.warn,
  };

  const redirectConsole = (type) => {
    return (...args) => {
      originalConsole[type](...args);
      const message = args
        .map((arg) => {
          if (typeof arg === 'object' && arg !== null) {
            try {
              return JSON.stringify(arg, null, 2);
            } catch (e) {
              return '[Unserializable Object]';
            }
          }
          return String(arg);
        })
        .join(' ');

      const line = document.createElement('div');
      line.textContent = message;
      line.style.color =
        type === 'error' ? 'red' : type === 'warn' ? 'orange' : 'inherit';
      line.style.borderBottom = '1px solid rgba(0,0,0,0.1)';
      line.style.padding =
        'calc(2px * var(--ui-scale, 1)) calc(4px * var(--ui-scale, 1))';
      consoleOutput.appendChild(line);
      consoleOutput.scrollTop = consoleOutput.scrollHeight;
    };
  };

  console.log = redirectConsole('log');
  console.error = redirectConsole('error');
  console.warn = redirectConsole('warn');

  console.log('JotCAD Workbench initialized. Console redirection active.');

  // Global Event Logger for Debugging
  const eventsToLog = [
    'pointerdown',
    'pointermove',
    'pointerup',
    'pointercancel',
    'gotpointercapture',
    'lostpointercapture',
    'touchstart',
    'touchmove',
    'touchend',
    'touchcancel',
    'mousedown',
    'mousemove',
    'mouseup',
  ];

  eventsToLog.forEach((evtName) => {
    window.addEventListener(
      evtName,
      (e) => {
        // Avoid flooding with move events unless we are looking for something specific,
        // but log them for now as requested.
        const targetStr = e.target
          ? `${e.target.tagName}${e.target.id ? '#' + e.target.id : ''}${
              e.target.className ? '.' + e.target.className : ''
            }`
          : 'unknown';
        const coords = e.touches
          ? `T:${e.touches.length}`
          : `P:${e.clientX},${e.clientY}`;

        // Use warn for visibility, and maybe skip move events if they are too noisy
        if (!evtName.includes('move')) {
          console.warn(
            `[EVENT] ${evtName} | Target: ${targetStr} | Coords: ${coords} | pointerId: ${
              e.pointerId || 'N/A'
            }`
          );
        }
      },
      { capture: true, passive: true }
    );
  });

  // 2. Scene setup
  const scene = new THREE.Scene();
  scene.background = new THREE.Color(0xcccccc);

  const furnitureGroup = new THREE.Group();
  scene.add(furnitureGroup);

  const modelGroup = new THREE.Group();
  scene.add(modelGroup);

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
  renderer.setPixelRatio(window.devicePixelRatio);
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

  // Dual Panning Mode Logic
  let isFocusPanning = false;
  const focusPanStart = new THREE.Vector2();
  const initialTarget = new THREE.Vector3();

  const onPointerDown = (e) => {
    // Mode 2: Shift + Right Click (or Shift + Left Click if preferred, but user said "pan")
    // button 2 is Right Click
    if (e.shiftKey && (e.button === 2 || e.button === 0)) {
      isFocusPanning = true;
      focusPanStart.set(e.clientX, e.clientY);
      initialTarget.copy(controls.target);
      controls.enabled = false;
      window.addEventListener('pointermove', onPointerMove);
      window.addEventListener('pointerup', onPointerUp);
      e.preventDefault();
      e.stopPropagation();
    }
  };

  const onPointerMove = (e) => {
    if (!isFocusPanning) return;

    const dx = e.clientX - focusPanStart.x;
    const dy = e.clientY - focusPanStart.y;

    // Project screen-space movement into world-space movement for the target
    const v = new THREE.Vector3();
    const cameraDir = new THREE.Vector3();
    camera.getWorldDirection(cameraDir);
    const up = camera.up.clone();
    const right = new THREE.Vector3().crossVectors(cameraDir, up).normalize();
    up.crossVectors(right, cameraDir).normalize();

    const distance = camera.position.distanceTo(controls.target);

    // Approximate scaling factor for screen to world
    let factor;
    if (camera instanceof THREE.PerspectiveCamera) {
      factor =
        (2.0 * Math.tan((camera.fov * Math.PI) / 360.0) * distance) /
        renderer.domElement.clientHeight;
    } else {
      const frustumHeight = camera.top - camera.bottom;
      factor = frustumHeight / renderer.domElement.clientHeight;
    }

    const worldDx = -dx * factor;
    const worldDy = dy * factor;

    controls.target
      .copy(initialTarget)
      .addScaledVector(right, worldDx)
      .addScaledVector(up, worldDy);
    controls.update();
  };

  const onPointerUp = () => {
    isFocusPanning = false;
    controls.enabled = true;
    window.removeEventListener('pointermove', onPointerMove);
    window.removeEventListener('pointerup', onPointerUp);
  };

  renderer.domElement.addEventListener('pointerdown', onPointerDown);
  // Prevent context menu when shift-right-clicking
  renderer.domElement.addEventListener('contextmenu', (e) => {
    if (e.shiftKey) e.preventDefault();
  });

  // Orientation Guide
  const axesScene = new THREE.Scene();
  const axesCamera = new THREE.PerspectiveCamera(50, 1, 0.1, 10);
  axesCamera.position.set(-2, -2, 2);
  axesCamera.up.set(0, 0, 1);
  axesCamera.lookAt(0, 0, 0);
  const axesRenderer = new THREE.WebGLRenderer({ alpha: true });
  axesRenderer.setPixelRatio(window.devicePixelRatio);
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
      renderer.setPixelRatio(window.devicePixelRatio);
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
  resizeObserver.observe(document.body);
  window.addEventListener('resize', updateRendererSize);

  if (window.visualViewport) {
    window.visualViewport.addEventListener('resize', updateRendererSize);
    window.visualViewport.addEventListener('scroll', updateRendererSize);
  }

  // 4. Logic & Events
  if (githubTokenHelp && helpWindow) {
    githubTokenHelp.addEventListener('click', () => {
      helpWindow.toggleMinimized();
    });
  }

  if (fetchDesignsButton) {
    fetchDesignsButton.addEventListener('click', fetchGithubDesigns);
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
        await writeEditor(content, false);
        await designStorage.saveSnapshot(fullPath, content);
        await updateUnpushedIndicator();
        showToast('Pulled from GitHub');
      } else {
        showToast('File not found');
      }
    } catch (e) {
      reportError('Pull failed', e);
    } finally {
      loadingIndicator.style.display = 'none';
    }
  });

  githubSaveButton.addEventListener('click', async () => {
    const token = githubTokenInput.value.trim();
    const repo = githubRepoInput.value.trim();
    const fullPath = githubPathInput.value.trim();
    const lastSlash = fullPath.lastIndexOf('/');
    const path = lastSlash !== -1 ? fullPath.substring(0, lastSlash) : '';
    const filename =
      lastSlash !== -1 ? fullPath.substring(lastSlash + 1) : fullPath;

    try {
      loadingIndicator.style.display = 'block';
      await prettifyCode();
      const content = readEditor();
      await githubStorage.saveFile(token, repo, path, filename, content);
      await designStorage.saveSnapshot(fullPath, content);
      await updateUnpushedIndicator();
      showToast('Committed to GitHub');
    } catch (e) {
      reportError('Commit failed', e);
    } finally {
      loadingIndicator.style.display = 'none';
    }
  });

  codeEditor.addEventListener('input', async () => {
    const fullPath = githubPathInput.value.trim();
    const content = readEditor();
    await designStorage.saveLocal(fullPath, content);
    await updateUnpushedIndicator();
  });

  const clearScene = () => {
    while (modelGroup.children.length > 0) {
      const obj = modelGroup.children[0];
      if (obj.geometry) obj.geometry.dispose();
      if (obj.material)
        Array.isArray(obj.material)
          ? obj.material.forEach((m) => m.dispose())
          : obj.material.dispose();
      modelGroup.remove(obj);
    }
  };

  let currentJotText = '';
  const renderJotString = async (text) => {
    console.log('JOT Content Received:', text);
    currentJotText = text;
    clearScene();
    try {
      await renderJotToThreejsScene(text, modelGroup, {
        edgeThresholdAngle: parseFloat(edgeThresholdInput.value),
      });
      const box = new THREE.Box3().setFromObject(modelGroup);
      if (box.isEmpty()) return;

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

  const refreshFileList = async () => {
    if (!fileList || !sessionIdInput.value) return;
    try {
      const res = await fetch(
        `${serverAddressInput.value}/list/${sessionIdInput.value}`
      );
      if (res.ok) {
        const files = await res.json();
        fileList.innerHTML = '';
        files.forEach((file) => {
          const btn = document.createElement('button');
          btn.textContent = file;
          btn.style.fontSize = '10px';
          btn.style.padding = '2px 6px';
          btn.onclick = () => {
            window.open(
              `${serverAddressInput.value}/get/${sessionIdInput.value}/${file}`,
              '_blank'
            );
          };
          fileList.appendChild(btn);
        });
      }
    } catch (e) {
      console.error('Failed to list files:', e);
    }
  };

  renderButton.addEventListener('click', async () => {
    loadingIndicator.style.display = 'block';
    try {
      await prettifyCode();
      const code = `${readEditor().trim()}.jot('${outputFilenameInput.value}')`;
      console.log(`Sending code to Jot server: ${serverAddressInput.value}`);
      const res = await fetch(
        `${serverAddressInput.value}/run/${sessionIdInput.value}/${outputFilenameInput.value}`,
        {
          method: 'POST',
          body: code,
        }
      );
      if (res.ok) {
        const jotResponse = await res.text();
        console.log('Received response from Jot server. Rendering...');
        await renderJotString(jotResponse);
        console.log('JotCAD model rendered successfully.');
        await refreshFileList();
      } else {
        const errorText = await res.text();
        console.error(`Error from Jot server: ${res.status} - ${errorText}`);
        showToast(`Server error: ${res.status}`);
      }
    } catch (e) {
      console.error('Failed to communicate with Jot server or render:', e);
      showToast('Render failed: ' + e.message);
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
  ].forEach((el) => {
    el.addEventListener('input', () => {
      localStorage.setItem('jotcad-' + el.id, el.value);
      if (el.id === 'sessionId') refreshFileList();
    });
  });

  githubPathInput.addEventListener('change', () => {
    const fullPath = githubPathInput.value.trim();
    localStorage.setItem('jotcad-githubPath', fullPath);
    loadDesign(fullPath);
  });

  codeEditor.addEventListener('keydown', (e) => {
    if (e.shiftKey && e.key === 'Enter') {
      e.preventDefault();
      renderButton.click();
    }
  });

  await refreshFileList();
  await loadDesign(githubPathInput.value.trim());
  await fetchGithubDesigns();
}

init();
