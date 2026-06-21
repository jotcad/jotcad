import { createSignal, onCleanup } from 'solid-js';

/**
 * Custom hook to handle infinite blackboard navigation.
 */
export const createBlackboardControls = (canvasRef, dragRef, setView, isWorldMode) => {
  let lastTouchState = { distance: 0, x: 0, y: 0 };
  let isPanning = false;

  const handleWheel = (e) => {
    if (e.target.closest('.viewport-container')) return;
    const worldIntent = isWorldMode() || e.ctrlKey;
    const win = e.target.closest('.jot-window');
    const isOverWindow = !!win;
    const isMaximized = win?.classList.contains('is-maximized');

    // If over a window, only zoom if:
    // 1. worldIntent is active (Ctrl/Shift/Alt)
    // 2. OR the window is maximized AND the user is NOT over a scrollable element
    if (isOverWindow && !worldIntent) {
        if (!isMaximized) return;
        
        // Check if over a scrollable area (textarea or something with custom-scrollbar)
        const isScrollable = e.target.closest('textarea, .custom-scrollbar, .viewport-container');
        if (isScrollable) return;
    }

    e.preventDefault();
    const delta = e.deltaY;
    const scaleFactor = delta > 0 ? 0.9 : 1.1;
    
    setView(prev => {
        const newScale = Math.min(Math.max(prev.scale * scaleFactor, 0.1), 5);
        const rect = canvasRef.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        const worldX = (mouseX - prev.x) / prev.scale;
        const worldY = (mouseY - prev.y) / prev.scale;

        const newX = mouseX - worldX * newScale;
        const newY = mouseY - worldY * newScale;

        return { x: newX, y: newY, scale: newScale };
    });
  };

  const handleTouchStart = (e) => {
    if (e.target.closest('.viewport-container')) return;
    if (e.touches.length === 2) {
      lastTouchState.distance = 0;
    }
  };

  const handleTouchEnd = (e) => {
    if (e.touches.length < 2) {
      lastTouchState.distance = 0;
    }
  };

  const handleTouchMove = (e) => {
      if (e.target.closest('.viewport-container')) return;
      if (e.touches.length === 2) {
        e.preventDefault();
        const t1 = e.touches[0];
        const t2 = e.touches[1];
        const dist = Math.hypot(t1.clientX - t2.clientX, t1.clientY - t2.clientY);
        const centerX = (t1.clientX + t2.clientX) / 2;
        const centerY = (t1.clientY + t2.clientY) / 2;

        if (lastTouchState.distance === 0) {
          lastTouchState = { distance: dist, x: centerX, y: centerY };
          return;
        }

        const scaleFactor = dist / lastTouchState.distance;
        
        setView(prev => {
            const newScale = Math.min(Math.max(prev.scale * scaleFactor, 0.1), 5);
            const rect = canvasRef.getBoundingClientRect();
            const localX = centerX - rect.left;
            const localY = centerY - rect.top;
            const dx = centerX - lastTouchState.x;
            const dy = centerY - lastTouchState.y;
            const worldX = (localX - dx - prev.x) / prev.scale;
            const worldY = (localY - dy - prev.y) / prev.scale;
            const newX = localX - worldX * newScale;
            const newY = localY - worldY * newScale;

            return { x: newX, y: newY, scale: newScale };
        });

        lastTouchState = { distance: dist, x: centerX, y: centerY };
      }
  };

  const handlePointerDown = (e) => {
    if (e.target.closest('.viewport-container')) return;
    const isMiddle = e.button === 1;
    const isModDrag = isWorldMode() && e.button === 0;
    const isBackground = e.target === dragRef && e.button === 0;

    if (isMiddle || isModDrag || isBackground) {
      isPanning = true;
      window.addEventListener('pointermove', handlePointerMove);
      window.addEventListener('pointerup', handlePointerUp);
      dragRef.setPointerCapture(e.pointerId);
      if (isModDrag) e.stopPropagation();
    }
  };

  const handlePointerMove = (e) => {
    if (isPanning) {
      setView((v) => ({ ...v, x: v.x + e.movementX, y: v.y + e.movementY }));
    }
  };

  const handlePointerUp = () => { 
      isPanning = false; 
      window.removeEventListener('pointermove', handlePointerMove);
      window.removeEventListener('pointerup', handlePointerUp);
  };

  return {
      attach() {
        canvasRef.addEventListener('wheel', handleWheel, { passive: false });
        canvasRef.addEventListener('touchmove', handleTouchMove, { passive: false });
        canvasRef.addEventListener('touchstart', handleTouchStart, { passive: true });
        canvasRef.addEventListener('touchend', handleTouchEnd, { passive: true });
        canvasRef.addEventListener('touchcancel', handleTouchEnd, { passive: true });
        window.addEventListener('pointerdown', handlePointerDown, { capture: true });
        canvasRef.addEventListener('mousedown', (e) => { if(e.button === 1) e.preventDefault(); });
      },
      detach() {
        window.removeEventListener('pointerdown', handlePointerDown, { capture: true });
        canvasRef.removeEventListener('wheel', handleWheel);
        canvasRef.removeEventListener('touchmove', handleTouchMove);
        canvasRef.removeEventListener('touchstart', handleTouchStart);
        canvasRef.removeEventListener('touchend', handleTouchEnd);
        canvasRef.removeEventListener('touchcancel', handleTouchEnd);
      }
  };
};
