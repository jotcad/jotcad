// components.js

/**
 * <jot-window title="Title" x="10px" y="10px">
 * A draggable, transparent floating window with glass effect.
 */
class JotWindow extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this._onResize = () => this.ensureVisibility();
    this.state = { x: 10, y: 50, width: 300, height: 'auto', minimized: false };
  }

  static get observedAttributes() {
    return ['minimized'];
  }

  attributeChangedCallback(name, oldVal, newVal) {
    if (name === 'minimized') {
      window.dispatchEvent(
        new CustomEvent('jot-window-state-changed', { detail: this })
      );
    }
  }

  connectedCallback() {
    const title = this.getAttribute('title') || 'Window';
    const storageKey = `jotcad-window-${title}`;
    const savedStateStr = localStorage.getItem(storageKey);
    const savedState = savedStateStr ? JSON.parse(savedStateStr) : null;

    const parseDim = (val, def) => {
      if (val === undefined || val === null) return def;
      const n = parseFloat(val);
      return isNaN(n) ? def : n;
    };

    let x = parseDim(savedState?.left, parseDim(this.getAttribute('x'), 10));
    let y = parseDim(savedState?.top, parseDim(this.getAttribute('y'), 50));
    let width = parseDim(
      savedState?.width,
      parseDim(this.getAttribute('width'), 300)
    );
    let height = savedState?.height || this.getAttribute('height') || 'auto';
    if (height !== 'auto') height = parseDim(height, 'auto');

    const minimized =
      savedState?.minimized !== undefined
        ? savedState.minimized
        : this.hasAttribute('minimized');

    // Sensible defaults for small screens if no saved state
    if (!savedState && window.innerWidth < 768) {
      if (width > window.innerWidth - 20) {
        width = window.innerWidth - 40;
        x = 20;
      }
    }

    this.state = { x, y, width, height, minimized };

    if (minimized) {
      this.setAttribute('minimized', '');
    } else {
      this.removeAttribute('minimized');
    }

    this.shadowRoot.innerHTML = `
      <style>
        :host {
          position: absolute;
          z-index: 100;
          display: flex;
          flex-direction: column;
          background: rgba(255, 255, 255, 0.4);
          border: 1px solid rgba(255, 255, 255, 0.3);
          border-radius: calc(8px * var(--ui-scale, 1));
          box-shadow: 0 4px 15px rgba(0,0,0,0.1);
          overflow: hidden;
          font-family: sans-serif;
          transition: opacity 0.2s;
          transform-origin: top left;
          touch-action: none;
          font-size: calc(13px * var(--ui-scale, 1));
        }
        :host([minimized]) {
          opacity: 0;
          pointer-events: none;
          transform: translateY(calc(-20px * var(--ui-scale, 1))) scale(0.5);
        }
        .title-bar {
          background: rgba(0, 0, 0, 0.05);
          padding: 0.6em 0.9em;
          cursor: move;
          font-weight: bold;
          font-size: 1em;
          display: flex;
          justify-content: space-between;
          border-bottom: 1px solid rgba(0,0,0,0.05);
          user-select: none;
          touch-action: none;
          transition: background-color 0.2s ease;
          position: relative;
        }
        .title-bar.drag-ready {
          background: rgba(64, 128, 255, 0.3);
        }
        .content {
          padding: 0.9em;
          flex: 1;
          overflow-y: auto;
          max-height: 80vh;
          display: flex;
          flex-direction: column;
        }
        .controls { display: flex; gap: 0.6em; z-index: 103; position: relative; }
        .btn { cursor: pointer; opacity: 0.6; font-size: 1.2em; line-height: 1; }
        .btn:hover { opacity: 1; }
        .resizer { touch-action: none; position: absolute; z-index: 101; width: 16px; height: 16px; }
      </style>
      <div class="title-bar" id="handle">
        <span>${title}</span>
        <div class="controls">
          <span class="btn" id="minimize">×</span>
        </div>
      </div>
      <div class="content">
        <slot></slot>
      </div>
      <!-- Resize Anchors -->
      <div class="resizer tl" style="top: 0; left: 0; cursor: nw-resize;"></div>
      <div class="resizer tr" style="top: 0; right: 0; cursor: ne-resize;"></div>
      <div class="resizer bl" style="bottom: 0; left: 0; cursor: sw-resize;"></div>
      <div class="resizer br" style="bottom: 0; right: 0; cursor: se-resize; background: linear-gradient(135deg, transparent 50%, rgba(0,0,0,0.1) 50%);"></div>
    `;

    this.renderStyles();
    this.ensureVisibility();
    window.addEventListener('resize', this._onResize);

    this.setupDragging();
    this.setupMultiResizing();

    this.shadowRoot.getElementById('minimize').onclick = (e) => {
      e.stopPropagation();
      this.toggleMinimized();
    };

    window.dispatchEvent(
      new CustomEvent('jot-window-registered', { detail: this })
    );
  }

  disconnectedCallback() {
    window.removeEventListener('resize', this._onResize);
  }

  renderStyles() {
    const s = 'var(--ui-scale, 1)';
    this.style.left = `calc(${this.state.x}px * ${s})`;
    this.style.top = `calc(${this.state.y}px * ${s})`;
    this.style.width = `calc(${this.state.width}px * ${s})`;
    if (this.state.height === 'auto') {
      this.style.height = 'auto';
    } else {
      this.style.height = `calc(${this.state.height}px * ${s})`;
    }
  }

  saveState() {
    const title = this.getAttribute('title') || 'Window';
    const state = {
      left: this.state.x + 'px',
      top: this.state.y + 'px',
      width: this.state.width + 'px',
      height: this.state.height === 'auto' ? 'auto' : this.state.height + 'px',
      minimized: this.hasAttribute('minimized'),
    };
    localStorage.setItem(`jotcad-window-${title}`, JSON.stringify(state));
  }

  ensureVisibility() {
    requestAnimationFrame(() => {
      const uiScaleVar = getComputedStyle(document.documentElement)
        .getPropertyValue('--ui-scale')
        .trim();
      const scale = uiScaleVar ? parseFloat(uiScaleVar) : 1.0;

      let { x, y, width } = this.state;
      const rect = this.getBoundingClientRect();
      const visualW = rect.width;

      const titleBarHeight = 35 * scale;
      const minVisible = 50 * scale;
      const dockHeight = 40 * scale;

      // We work in visual coordinates for constraints
      let visualX = x * scale;
      let visualY = y * scale;

      // 1. Constrain Top
      if (visualY < dockHeight) visualY = dockHeight;
      if (visualY > window.innerHeight - titleBarHeight)
        visualY = window.innerHeight - titleBarHeight;

      // 2. Constrain Left/Right
      if (visualX + visualW < minVisible) visualX = minVisible - visualW;
      if (visualX > window.innerWidth - minVisible)
        visualX = window.innerWidth - minVisible;

      // 3. Constrain Width
      if (visualW > window.innerWidth) {
        // If window is too wide, we adjust unscaled width
        this.state.width = (window.innerWidth - 20) / scale;
        if (visualX + window.innerWidth - 20 > window.innerWidth) {
          visualX = 10;
        }
      }

      this.state.x = visualX / scale;
      this.state.y = visualY / scale;

      this.renderStyles();
    });
  }

  toggleMinimized() {
    if (this.hasAttribute('minimized')) {
      this.removeAttribute('minimized');
    } else {
      this.setAttribute('minimized', '');
    }
    this.saveState();
  }

  setupDragging() {
    const handle = this.shadowRoot.getElementById('handle');
    let isDragging = false;
    let isDragReady = false;
    let startX, startY, initialLeft, initialTop;
    let activePointerId = null;
    let longPressTimer = null;
    let currentScale = 1;

    handle.onselectstart = () => false;
    handle.ondragstart = (e) => e.preventDefault();

    handle.addEventListener('contextmenu', (e) => {
      e.preventDefault();
      return false;
    });

    const onPointerDown = (e) => {
      e.preventDefault();

      const path = e.composedPath();
      const isClickOnHandle = path.includes(handle);
      if (!isClickOnHandle || e.target.classList.contains('btn')) {
        return;
      }

      e.stopPropagation();

      isDragging = true;
      activePointerId = e.pointerId;

      const uiScaleVar = getComputedStyle(document.documentElement)
        .getPropertyValue('--ui-scale')
        .trim();
      currentScale = (uiScaleVar ? parseFloat(uiScaleVar) : 1.0) || 1.0;

      startX = e.clientX;
      startY = e.clientY;
      initialLeft = this.state.x;
      initialTop = this.state.y;

      handle.setPointerCapture(activePointerId);
      handle.addEventListener('pointermove', onPointerMove);
      handle.addEventListener('pointerup', onPointerUp);
      handle.addEventListener('pointercancel', onPointerUp);

      const isTouch = e.pointerType === 'touch' || e.pointerType === 'pen';

      if (isTouch) {
        isDragReady = false;
        longPressTimer = setTimeout(() => {
          isDragReady = true;
          handle.classList.add('drag-ready');
          this.style.zIndex = 1000;
          document.body.style.userSelect = 'none';
        }, 600);
      } else {
        isDragReady = true;
        this.style.zIndex = 1000;
        document.body.style.userSelect = 'none';
      }
    };

    const onPointerMove = (e) => {
      if (!isDragging || e.pointerId !== activePointerId) return;

      const dx = e.clientX - startX;
      const dy = e.clientY - startY;

      if (!isDragReady) {
        if (Math.abs(dx) > 10 || Math.abs(dy) > 10) {
          clearTimeout(longPressTimer);
          isDragging = false;
          handle.releasePointerCapture(activePointerId);
          handle.removeEventListener('pointermove', onPointerMove);
          handle.removeEventListener('pointerup', onPointerUp);
          handle.removeEventListener('pointercancel', onPointerUp);
        }
        return;
      }

      let newTop = initialTop + dy / currentScale;
      if (newTop < 40) newTop = 40;

      this.state.x = initialLeft + dx / currentScale;
      this.state.y = newTop;
      this.renderStyles();

      e.preventDefault();
      e.stopPropagation();
    };

    const onPointerUp = (e) => {
      if (e.pointerId !== activePointerId) return;

      if (longPressTimer) clearTimeout(longPressTimer);
      handle.classList.remove('drag-ready');

      try {
        handle.releasePointerCapture(activePointerId);
      } catch (err) {}

      handle.removeEventListener('pointermove', onPointerMove);
      handle.removeEventListener('pointerup', onPointerUp);
      handle.removeEventListener('pointercancel', onPointerUp);

      if (isDragging && isDragReady) this.saveState();

      isDragging = false;
      isDragReady = false;
      activePointerId = null;
      document.body.style.userSelect = 'auto';
    };

    handle.addEventListener('pointerdown', onPointerDown);
  }

  setupMultiResizing() {
    const resizers = this.shadowRoot.querySelectorAll('.resizer');
    let activeResizer = null;
    let activePointerId = null;
    let startX, startY, initialW, initialH, initialX, initialY;
    let currentScale = 1;

    resizers.forEach((r) => {
      r.addEventListener('contextmenu', (e) => {
        e.preventDefault();
        return false;
      });
      r.onselectstart = () => false;
      r.ondragstart = (e) => e.preventDefault();
    });

    const onPointerDown = (e) => {
      e.preventDefault();
      e.stopPropagation();
      activeResizer = e.target;
      activePointerId = e.pointerId;
      startX = e.clientX;
      startY = e.clientY;

      const uiScaleVar = getComputedStyle(document.documentElement)
        .getPropertyValue('--ui-scale')
        .trim();
      currentScale = (uiScaleVar ? parseFloat(uiScaleVar) : 1.0) || 1.0;

      initialW = this.state.width;
      initialH =
        this.state.height === 'auto'
          ? this.getBoundingClientRect().height / currentScale
          : this.state.height;
      initialX = this.state.x;
      initialY = this.state.y;

      document.body.style.userSelect = 'none';

      activeResizer.setPointerCapture(activePointerId);
      activeResizer.addEventListener('pointermove', onPointerMove);
      activeResizer.addEventListener('pointerup', onPointerUp);
      activeResizer.addEventListener('pointercancel', onPointerUp);
    };

    const onPointerMove = (e) => {
      if (!activeResizer || e.pointerId !== activePointerId) return;
      e.preventDefault();
      e.stopPropagation();

      const dx = (e.clientX - startX) / currentScale;
      const dy = (e.clientY - startY) / currentScale;

      const isTop =
        activeResizer.classList.contains('tl') ||
        activeResizer.classList.contains('tr');
      const isLeft =
        activeResizer.classList.contains('tl') ||
        activeResizer.classList.contains('bl');
      const isRight =
        activeResizer.classList.contains('tr') ||
        activeResizer.classList.contains('br');
      const isBottom =
        activeResizer.classList.contains('bl') ||
        activeResizer.classList.contains('br');

      if (isRight) this.state.width = initialW + dx;
      if (isBottom) this.state.height = initialH + dy;

      if (isLeft) {
        this.state.width = initialW - dx;
        this.state.x = initialX + dx;
      }
      if (isTop) {
        let newTop = initialY + dy;
        if (newTop < 40) newTop = 40;
        // Correct height for clamped top
        const actualDy = newTop - initialY;
        this.state.height = initialH - actualDy;
        this.state.y = newTop;
      }
      this.renderStyles();
    };

    const onPointerUp = (e) => {
      if (e.pointerId !== activePointerId) return;

      activeResizer.releasePointerCapture(activePointerId);
      activeResizer.removeEventListener('pointermove', onPointerMove);
      activeResizer.removeEventListener('pointerup', onPointerUp);
      activeResizer.removeEventListener('pointercancel', onPointerUp);

      if (activeResizer) this.saveState();
      activeResizer = null;
      activePointerId = null;
      document.body.style.userSelect = 'auto';
    };

    resizers.forEach((r) => {
      r.addEventListener('pointerdown', onPointerDown);
    });
  }
}

/**
 * <jot-dock>
 * A top bar that holds persistent window labels.
 */
class JotDock extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
    this.windowMap = new Map();
  }

  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host {
          position: fixed;
          top: 0; left: 0; right: 0;
          height: calc(40px * var(--ui-scale, 1));
          background: rgba(0, 0, 0, 0.2);
          display: flex;
          align-items: center;
          padding: 0 calc(10px * var(--ui-scale, 1));
          gap: calc(10px * var(--ui-scale, 1));
          z-index: 2000;
          border-bottom: 1px solid rgba(255,255,255,0.1);
          color: white;
          font-family: sans-serif;
          font-size: calc(13px * var(--ui-scale, 1));
          transform-origin: top left;
          width: 100%;
        }
        .brand { font-weight: bold; margin-right: 10px; opacity: 0.8; }
        #items { display: flex; gap: 8px; }
        .dock-item {
          background: rgba(255, 255, 255, 0.3);
          padding: 0.3em 0.9em;
          border-radius: 4px;
          cursor: pointer;
          white-space: nowrap;
          border: 1px solid rgba(255,255,255,0.1);
          transition: all 0.2s;
          user-select: none;
        }
        .dock-item:hover { background: rgba(255, 255, 255, 0.5); }
        .dock-item.active {
          background: rgba(255, 255, 255, 0.1);
          opacity: 0.6;
          border-style: dashed;
        }
      </style>
      <div class="brand">JotCAD</div>
      <div id="items"></div>
    `;

    window.addEventListener('jot-window-registered', (e) => {
      this.registerWindow(e.detail);
    });

    window.addEventListener('jot-window-state-changed', (e) => {
      this.updateWindowState(e.detail);
    });

    document
      .querySelectorAll('jot-window')
      .forEach((win) => this.registerWindow(win));
  }

  registerWindow(win) {
    if (this.windowMap.has(win)) return;

    const items = this.shadowRoot.getElementById('items');
    const btn = document.createElement('div');
    btn.className = 'dock-item';
    btn.textContent = win.getAttribute('title');
    btn.onclick = () => win.toggleMinimized();

    items.appendChild(btn);
    this.windowMap.set(win, btn);
    this.updateWindowState(win);
  }

  updateWindowState(win) {
    const btn = this.windowMap.get(win);
    if (!btn) return;

    if (win.hasAttribute('minimized')) {
      btn.classList.remove('active');
    } else {
      btn.classList.add('active');
    }
  }
}

class JotViewport extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }
  connectedCallback() {
    this.shadowRoot.innerHTML = `
      <style>
        :host { 
          display: block; 
          position: fixed; 
          top: 0; left: 0; right: 0; bottom: 0; 
          background: #ccc; 
          z-index: 0; 
        }
        #container { width: 100%; height: 100%; }
      </style>
      <div id="container"></div>
    `;
  }
  get container() {
    return this.shadowRoot.getElementById('container');
  }
}

/**
 * <jot-toast>
 * A temporary non-blocking notification.
 */
class JotToast extends HTMLElement {
  constructor() {
    super();
    this.attachShadow({ mode: 'open' });
  }

  show(message, duration = 3000) {
    const toast = document.createElement('div');
    toast.className = 'toast';
    toast.textContent = message;
    this.shadowRoot.innerHTML = `
      <style>
        .toast {
          position: fixed;
          bottom: calc(60px * var(--ui-scale, 1));
          left: 50%;
          transform: translateX(-50%);
          background: rgba(0, 0, 0, 0.7);
          color: white;
          padding: calc(8px * var(--ui-scale, 1)) calc(20px * var(--ui-scale, 1));
          border-radius: calc(20px * var(--ui-scale, 1));
          font-family: sans-serif;
          font-size: calc(13px * var(--ui-scale, 1));
          z-index: 3000;
          opacity: 0;
          transition: opacity 0.3s, transform 0.3s;
          pointer-events: none;
        }
        .toast.visible {
          opacity: 1;
          transform: translateX(-50%) translateY(calc(-10px * var(--ui-scale, 1)));
        }
      </style>
      <div id="container"></div>
    `;
    const container = this.shadowRoot.getElementById('container');
    container.appendChild(toast);

    requestAnimationFrame(() => toast.classList.add('visible'));

    setTimeout(() => {
      toast.classList.remove('visible');
      setTimeout(() => toast.remove(), 300);
    }, duration);
  }
}

customElements.define('jot-window', JotWindow);
customElements.define('jot-dock', JotDock);
customElements.define('jot-viewport', JotViewport);
customElements.define('jot-toast', JotToast);
