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

    const x = savedState?.left || this.getAttribute('x') || '10px';
    const y = savedState?.top || this.getAttribute('y') || '50px';
    const width = savedState?.width || this.getAttribute('width') || '300px';
    const height = savedState?.height || this.getAttribute('height') || 'auto';
    const minimized =
      savedState?.minimized !== undefined
        ? savedState.minimized
        : this.hasAttribute('minimized');

    this.style.left = x;
    this.style.top = y;
    this.style.width = width;
    this.style.height = height;
    if (minimized) {
      this.setAttribute('minimized', '');
    } else {
      this.removeAttribute('minimized');
    }

    // Ensure visible on init and resize
    this.ensureVisibility();
    window.addEventListener('resize', this._onResize);

    this.shadowRoot.innerHTML = `
      <style>
        :host {
          position: absolute;
          z-index: 100;
          display: flex;
          flex-direction: column;
          background: rgba(255, 255, 255, 0.4);
          border: 1px solid rgba(255, 255, 255, 0.3);
          border-radius: 8px;
          box-shadow: 0 4px 15px rgba(0,0,0,0.1);
          overflow: hidden;
          font-family: sans-serif;
          transition: transform 0.3s cubic-bezier(0.4, 0, 0.2, 1), opacity 0.2s, scale 0.3s;
          transform-origin: top center;
        }
        :host([minimized]) {
          opacity: 0;
          scale: 0.5;
          pointer-events: none;
          transform: translateY(-20px);
        }
        .title-bar {
          background: rgba(0, 0, 0, 0.05);
          padding: 8px 12px;
          cursor: move;
          font-weight: bold;
          font-size: 13px;
          display: flex;
          justify-content: space-between;
          border-bottom: 1px solid rgba(0,0,0,0.05);
          user-select: none;
        }
        .content {
          padding: 12px;
          flex: 1;
          overflow-y: auto;
          max-height: 80vh;
          display: flex;
          flex-direction: column;
        }
        .controls { display: flex; gap: 8px; }
        .btn { cursor: pointer; opacity: 0.6; font-size: 16px; line-height: 1; }
        .btn:hover { opacity: 1; }
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
      <div class="resizer tl" style="position: absolute; top: 0; left: 0; width: 10px; height: 10px; cursor: nw-resize; z-index: 101;"></div>
      <div class="resizer tr" style="position: absolute; top: 0; right: 0; width: 10px; height: 10px; cursor: ne-resize; z-index: 101;"></div>
      <div class="resizer bl" style="position: absolute; bottom: 0; left: 0; width: 10px; height: 10px; cursor: sw-resize; z-index: 101;"></div>
      <div class="resizer br" style="position: absolute; bottom: 0; right: 0; width: 10px; height: 10px; cursor: se-resize; z-index: 101; background: linear-gradient(135deg, transparent 50%, rgba(0,0,0,0.1) 50%);"></div>
    `;

    this.setupDragging();
    this.setupMultiResizing();

    this.shadowRoot.getElementById('minimize').onclick = (e) => {
      e.stopPropagation();
      this.toggleMinimized();
    };

    // Register with dock if it exists
    window.dispatchEvent(
      new CustomEvent('jot-window-registered', { detail: this })
    );
  }

  disconnectedCallback() {
    window.removeEventListener('resize', this._onResize);
  }

  saveState() {
    const title = this.getAttribute('title') || 'Window';
    const state = {
      left: this.style.left,
      top: this.style.top,
      width: this.style.width,
      height: this.style.height,
      minimized: this.hasAttribute('minimized'),
    };
    localStorage.setItem(`jotcad-window-${title}`, JSON.stringify(state));
  }

  ensureVisibility() {
    // Wait for DOM to settle
    requestAnimationFrame(() => {
      const rect = this.getBoundingClientRect();
      const titleBarHeight = 35;
      const minVisible = 50;

      let top = parseFloat(this.style.top);
      let left = parseFloat(this.style.left);

      // Handle cases where style properties might not be set or are invalid
      if (isNaN(top)) top = rect.top;
      if (isNaN(left)) left = rect.left;

      // 1. Constrain Top (must be below dock at 40px)
      if (top < 40) top = 40;
      if (top > window.innerHeight - titleBarHeight)
        top = window.innerHeight - titleBarHeight;

      // 2. Constrain Left/Right (keep at least minVisible px of the title bar on screen)
      if (left + rect.width < minVisible) left = minVisible - rect.width;
      if (left > window.innerWidth - minVisible)
        left = window.innerWidth - minVisible;

      this.style.top = `${top}px`;
      this.style.left = `${left}px`;
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
    let startX, startY, initialLeft, initialTop;

    const onMouseDown = (e) => {
      if (e.target.classList.contains('btn')) return;
      isDragging = true;
      startX = e.clientX;
      startY = e.clientY;
      const rect = this.getBoundingClientRect();
      initialLeft = rect.left;
      initialTop = rect.top;
      this.style.zIndex = 1000;
      document.body.style.userSelect = 'none';

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    };

    const onMouseMove = (e) => {
      if (!isDragging) return;
      const dx = e.clientX - startX;
      const dy = e.clientY - startY;

      let newTop = initialLeft !== undefined ? initialTop + dy : e.clientY;
      if (newTop < 40) newTop = 40;

      this.style.left = `${initialLeft + dx}px`;
      this.style.top = `${newTop}px`;
    };

    const onMouseUp = () => {
      if (isDragging) this.saveState();
      isDragging = false;
      document.body.style.userSelect = 'auto';
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };

    handle.addEventListener('mousedown', onMouseDown);
  }

  setupMultiResizing() {
    const resizers = this.shadowRoot.querySelectorAll('.resizer');
    let activeResizer = null;
    let startX, startY, initialRect;

    const onMouseDown = (e) => {
      e.stopPropagation();
      activeResizer = e.target;
      startX = e.clientX;
      startY = e.clientY;
      initialRect = this.getBoundingClientRect();
      document.body.style.userSelect = 'none';

      window.addEventListener('mousemove', onMouseMove);
      window.addEventListener('mouseup', onMouseUp);
    };

    const onMouseMove = (e) => {
      if (!activeResizer) return;
      const dx = e.clientX - startX;
      const dy = e.clientY - startY;

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

      if (isRight) this.style.width = `${initialRect.width + dx}px`;
      if (isBottom) this.style.height = `${initialRect.height + dy}px`;

      if (isLeft) {
        this.style.width = `${initialRect.width - dx}px`;
        this.style.left = `${initialRect.left + dx}px`;
      }
      if (isTop) {
        let newTop = initialRect.top + dy;
        if (newTop < 40) newTop = 40;
        const actualDy = newTop - initialRect.top;
        this.style.height = `${initialRect.height - actualDy}px`;
        this.style.top = `${newTop}px`;
      }
    };

    const onMouseUp = () => {
      if (activeResizer) this.saveState();
      activeResizer = null;
      document.body.style.userSelect = 'auto';
      window.removeEventListener('mousemove', onMouseMove);
      window.removeEventListener('mouseup', onMouseUp);
    };

    resizers.forEach((r) => r.addEventListener('mousedown', onMouseDown));
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
          height: 40px;
          background: rgba(0, 0, 0, 0.2);
          display: flex;
          align-items: center;
          padding: 0 10px;
          gap: 10px;
          z-index: 2000;
          border-bottom: 1px solid rgba(255,255,255,0.1);
          color: white;
          font-family: sans-serif;
          font-size: 13px;
        }
        .brand { font-weight: bold; margin-right: 10px; opacity: 0.8; }
        #items { display: flex; gap: 8px; }
        .dock-item {
          background: rgba(255, 255, 255, 0.3);
          padding: 4px 12px;
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

    // Catch any windows already in DOM
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
        :host { display: block; position: fixed; top: 0; left: 0; width: 100vw; height: 100vh; background: #ccc; z-index: 0; }
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
          bottom: 60px;
          left: 50%;
          transform: translateX(-50%);
          background: rgba(0, 0, 0, 0.7);
          color: white;
          padding: 8px 20px;
          border-radius: 20px;
          font-family: sans-serif;
          font-size: 13px;
          z-index: 3000;
          opacity: 0;
          transition: opacity 0.3s, transform 0.3s;
          pointer-events: none;
        }
        .toast.visible {
          opacity: 1;
          transform: translateX(-50%) translateY(-10px);
        }
      </style>
      <div id="container"></div>
    `;
    const container = this.shadowRoot.getElementById('container');
    container.appendChild(toast);

    // Trigger animation
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
