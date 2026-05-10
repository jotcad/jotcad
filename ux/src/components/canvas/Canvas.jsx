import {
  createSignal,
  onMount,
  onCleanup,
  For,
  createMemo,
} from 'solid-js';
import { blackboard } from '../../lib/blackboard';
import { desktopIcons, windowActions, setPointerCount, dynamicOps } from '../../lib/state/AppState';

import { DesktopIcon } from '../system/desktop/DesktopIcon';
import { WindowManager } from '../system/desktop/WindowManager';
import { SyncBadge } from '../system/SyncBadge';

export const Canvas = () => {
  const [view, setView] = createSignal({ x: 0, y: 0, scale: 1 });
  const [isWorldMode, setIsWorldMode] = createSignal(false);
  
  // Make view state available globally for scale-aware math in other components
  if (typeof window !== 'undefined') window._JOT_VIEW = view;

  // Derived signal for user op icons
  const userOpIcons = createMemo(() => {
    const ops = dynamicOps();
    console.log('[Canvas] userOpIcons calculation. Dynamic Ops Count:', Object.keys(ops).length);
    if (Object.keys(ops).length > 0) {
      console.log('[Canvas] Dynamic Op Paths:', Object.keys(ops));
    }
    return Object.entries(ops).map(([path, data], i) => {
        // Use saved position from metadata if available, otherwise grid
        const savedPos = data.schema?._desktopPos || { 
            x: 300 + (i % 5) * 120, 
            y: 40 + Math.floor(i / 5) * 120 
        };
        return {
            id: `op_${path.replace(/[\/:]/g, '_')}`,
            type: 'editor',
            target: path,
            label: path.replace('user/', ''),
            icon: 'FileText',
            x: savedPos.x,
            y: savedPos.y
        };
    });
  });

  let canvasRef;
  let dragRef;
  let lastTouchState = { distance: 0, x: 0, y: 0 };
  let isPanning = false;

  onMount(() => {
    console.log('[Canvas] Mounting... Calling blackboard.start()');
    blackboard.start();

    const updatePointers = (e) => {
      if (!window._activePointers) window._activePointers = new Set();
      if (e.type === 'pointerdown') {
        window._activePointers.add(e.pointerId);
      } else {
        window._activePointers.delete(e.pointerId);
      }
      setPointerCount(window._activePointers.size);
    };

    const handleKeyDown = (e) => {
      if (e.shiftKey || e.altKey || e.metaKey) {
        setIsWorldMode(true);
      }
    };

    const handleKeyUp = (e) => {
      if (!e.shiftKey && !e.altKey && !e.metaKey) {
        setIsWorldMode(false);
      }
    };

    window.addEventListener('pointerdown', updatePointers);
    window.addEventListener('pointerup', updatePointers);
    window.addEventListener('pointercancel', updatePointers);
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    window.addEventListener('blur', () => setIsWorldMode(false));

    const handleWheel = (e) => {
      const worldIntent = isWorldMode() || e.ctrlKey;
      const isOverWindow = e.target.closest('.jot-window');
      if (isOverWindow && !worldIntent) return; 

      e.preventDefault();
      const delta = e.deltaY;
      const scaleFactor = delta > 0 ? 0.9 : 1.1;
      const newScale = Math.min(Math.max(view().scale * scaleFactor, 0.1), 5);

      const rect = canvasRef.getBoundingClientRect();
      const mouseX = e.clientX - rect.left;
      const mouseY = e.clientY - rect.top;

      const worldX = (mouseX - view().x) / view().scale;
      const worldY = (mouseY - view().y) / view().scale;

      const newX = mouseX - worldX * newScale;
      const newY = mouseY - worldY * newScale;

      setView({ x: newX, y: newY, scale: newScale });
    };

    const handleTouchStart = (e) => {
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
          const newScale = Math.min(Math.max(view().scale * scaleFactor, 0.1), 5);
          const rect = canvasRef.getBoundingClientRect();
          const localX = centerX - rect.left;
          const localY = centerY - rect.top;
          const dx = centerX - lastTouchState.x;
          const dy = centerY - lastTouchState.y;
          const worldX = (localX - dx - view().x) / view().scale;
          const worldY = (localY - dy - view().y) / view().scale;
          const newX = localX - worldX * newScale;
          const newY = localY - worldY * newScale;
  
          setView({ x: newX, y: newY, scale: newScale });
          lastTouchState = { distance: dist, x: centerX, y: centerY };
        }
    };

    const handlePointerDown = (e) => {
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

    canvasRef.addEventListener('wheel', handleWheel, { passive: false });
    canvasRef.addEventListener('touchmove', handleTouchMove, { passive: false });
    canvasRef.addEventListener('touchstart', handleTouchStart, { passive: true });
    canvasRef.addEventListener('touchend', handleTouchEnd, { passive: true });
    canvasRef.addEventListener('touchcancel', handleTouchEnd, { passive: true });
    
    window.addEventListener('pointerdown', handlePointerDown, { capture: true });
    canvasRef.addEventListener('mousedown', (e) => { if(e.button === 1) e.preventDefault(); });

    onCleanup(() => {
      window.removeEventListener('pointerdown', updatePointers);
      window.removeEventListener('pointerup', updatePointers);
      window.removeEventListener('pointercancel', updatePointers);
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
      window.removeEventListener('blur', () => setIsWorldMode(false));
      window.removeEventListener('pointerdown', handlePointerDown, { capture: true });
      canvasRef.removeEventListener('wheel', handleWheel);
      canvasRef.removeEventListener('touchmove', handleTouchMove);
      canvasRef.removeEventListener('touchstart', handleTouchStart);
      canvasRef.removeEventListener('touchend', handleTouchEnd);
      canvasRef.removeEventListener('touchcancel', handleTouchEnd);
    });
  });

  return (
    <div 
        class={`canvas-container w-full h-full overflow-hidden bg-blackboard relative select-none ${isWorldMode() ? 'cursor-grab active:cursor-grabbing' : ''}`} 
        ref={canvasRef}
    >
      <div ref={dragRef} class="pan-layer absolute inset-0 z-0 pointer-events-auto" />

      <div
        class="absolute inset-0 pointer-events-none"
        style={{
          transform: `translate(${view().x}px, ${view().y}px) scale(${view().scale})`,
          'transform-origin': '0 0',
        }}
      >
        <div class="absolute inset-0 pointer-events-auto">
            <For each={desktopIcons}>
                {(icon) => <DesktopIcon data={icon} />}
            </For>

            <For each={userOpIcons()} by="id">
                {(icon) => <DesktopIcon data={icon} isUserOp={true} />}
            </For>
            
            <WindowManager isSpatial={true} />
        </div>
      </div>

      <WindowManager isSpatial={false} />

      <div class="absolute bottom-6 left-6 flex flex-col gap-1 pointer-events-none z-[100]">
        <div class="text-[10px] font-black text-white/20 tracking-[0.4em] uppercase">
          JotCAD Spatial Desktop
        </div>
      </div>
    </div>
  );
};
