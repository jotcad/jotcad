import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { createBlackboardControls } from '../src/lib/blackboard/BlackboardControls';

describe('BlackboardControls Pinch-Zoom Isolation', () => {
  let activeCanvas = null;

  beforeEach(() => {
    // Mock pointer capture methods which JSDOM does not implement
    if (typeof Element !== 'undefined') {
      if (!Element.prototype.setPointerCapture) {
        Element.prototype.setPointerCapture = vi.fn();
      }
      if (!Element.prototype.releasePointerCapture) {
        Element.prototype.releasePointerCapture = vi.fn();
      }
    }
  });

  afterEach(() => {
    if (activeCanvas && activeCanvas.parentNode) {
      activeCanvas.parentNode.removeChild(activeCanvas);
    }
    activeCanvas = null;
  });

  const createMockElements = () => {
    const canvas = document.createElement('div');
    canvas.className = 'canvas-container';
    
    const drag = document.createElement('div');
    drag.className = 'pan-layer';
    canvas.appendChild(drag);

    const windowEl = document.createElement('div');
    windowEl.className = 'jot-window';
    canvas.appendChild(windowEl);

    const viewport = document.createElement('div');
    viewport.className = 'viewport-container';
    windowEl.appendChild(viewport);

    const contentInsideViewport = document.createElement('div');
    viewport.appendChild(contentInsideViewport);

    document.body.appendChild(canvas);
    activeCanvas = canvas;

    return { canvas, drag, windowEl, viewport, contentInsideViewport };
  };

  it('should allow blackboard zooming when trackpad pinch-zoom (wheel + ctrlKey) is outside viewport', () => {
    const { canvas, drag } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    const event = new WheelEvent('wheel', {
      bubbles: true,
      cancelable: true,
      ctrlKey: true,
      deltaY: 10,
      clientX: 50,
      clientY: 50,
    });
    drag.dispatchEvent(event);

    expect(setView).toHaveBeenCalled();
    controls.detach();
  });

  it('should block blackboard zooming when trackpad pinch-zoom (wheel + ctrlKey) is inside viewport', () => {
    const { canvas, drag, contentInsideViewport } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    const event = new WheelEvent('wheel', {
      bubbles: true,
      cancelable: true,
      ctrlKey: true,
      deltaY: 10,
      clientX: 50,
      clientY: 50,
    });
    contentInsideViewport.dispatchEvent(event);

    expect(setView).not.toHaveBeenCalled();
    controls.detach();
  });

  it('should allow blackboard zooming when touch pinch (2 touches) is outside viewport', () => {
    const { canvas, drag } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    // 1. Establish initial touch distance
    const startEvent = new TouchEvent('touchstart', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 20, clientY: 20 }
      ]
    });
    drag.dispatchEvent(startEvent);

    // 2. Perform first move to set initial distance internally
    const moveEvent1 = new TouchEvent('touchmove', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 20, clientY: 20 }
      ]
    });
    drag.dispatchEvent(moveEvent1);

    // 3. Perform second move to calculate delta zoom
    const moveEvent2 = new TouchEvent('touchmove', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 30, clientY: 30 }
      ]
    });
    drag.dispatchEvent(moveEvent2);

    expect(setView).toHaveBeenCalled();
    controls.detach();
  });

  it('should block blackboard zooming when touch pinch (2 touches) is inside viewport', () => {
    const { canvas, drag, contentInsideViewport } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    // 1. Establish initial touch distance
    const startEvent = new TouchEvent('touchstart', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 20, clientY: 20 }
      ]
    });
    contentInsideViewport.dispatchEvent(startEvent);

    // 2. Perform first move
    const moveEvent1 = new TouchEvent('touchmove', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 20, clientY: 20 }
      ]
    });
    contentInsideViewport.dispatchEvent(moveEvent1);

    // 3. Perform second move (change distance)
    const moveEvent2 = new TouchEvent('touchmove', {
      bubbles: true,
      cancelable: true,
      touches: [
        { clientX: 10, clientY: 10 },
        { clientX: 30, clientY: 30 }
      ]
    });
    contentInsideViewport.dispatchEvent(moveEvent2);

    expect(setView).not.toHaveBeenCalled();
    controls.detach();
  });

  it('should allow blackboard panning when middle click drag starts outside viewport', () => {
    const { canvas, drag } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    const downEvent = new PointerEvent('pointerdown', {
      bubbles: true,
      button: 1, // Middle click
      pointerId: 1
    });
    drag.dispatchEvent(downEvent);

    const moveEvent = new PointerEvent('pointermove', {
      bubbles: true,
      movementX: 10,
      movementY: 10
    });
    window.dispatchEvent(moveEvent);

    expect(setView).toHaveBeenCalled();
    controls.detach();
  });

  it('should block blackboard panning when middle click drag starts inside viewport', () => {
    const { canvas, drag, contentInsideViewport } = createMockElements();
    const setView = vi.fn();
    const isWorldMode = () => false;

    const controls = createBlackboardControls(canvas, drag, setView, isWorldMode);
    controls.attach();

    const downEvent = new PointerEvent('pointerdown', {
      bubbles: true,
      button: 1, // Middle click
      pointerId: 1
    });
    contentInsideViewport.dispatchEvent(downEvent);

    const moveEvent = new PointerEvent('pointermove', {
      bubbles: true,
      movementX: 10,
      movementY: 10
    });
    window.dispatchEvent(moveEvent);

    expect(setView).not.toHaveBeenCalled();
    controls.detach();
  });
});
