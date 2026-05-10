import {
  createSignal,
  onMount,
  onCleanup,
  For,
  createMemo,
} from 'solid-js';
import { blackboard } from '../../lib/blackboard';
import { desktopIcons, windowActions } from '../../lib/state/DesktopState';
import { dynamicOps } from '../../lib/state/MeshState';
import { setPointerCount } from '../../lib/state/SystemState';
import { createBlackboardControls } from '../../lib/blackboard/BlackboardControls';

import { DesktopIcon } from '../system/desktop/DesktopIcon';
import { WindowManager } from '../system/desktop/WindowManager';

export const Canvas = () => {
  const [view, setView] = createSignal({ x: 0, y: 0, scale: 1 });
  const [isWorldMode, setIsWorldMode] = createSignal(false);
  
  if (typeof window !== 'undefined') window._JOT_VIEW = view;

  const userOpIcons = createMemo(() => {
    const ops = dynamicOps();
    console.log('[Canvas] userOpIcons updating. Count:', Object.keys(ops).length);
    return Object.entries(ops).map(([path, data], i) => {
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

  onMount(() => {
    console.log('[Canvas] Mounting...');
    blackboard.start();

    const controls = createBlackboardControls(canvasRef, dragRef, setView, isWorldMode);
    controls.attach();

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
      if (e.shiftKey || e.altKey || e.metaKey) setIsWorldMode(true);
    };
    const handleKeyUp = (e) => {
      if (!e.shiftKey && !e.altKey && !e.metaKey) setIsWorldMode(false);
    };

    window.addEventListener('pointerdown', updatePointers);
    window.addEventListener('pointerup', updatePointers);
    window.addEventListener('pointercancel', updatePointers);
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);
    window.addEventListener('blur', () => setIsWorldMode(false));

    onCleanup(() => {
      controls.detach();
      window.removeEventListener('pointerdown', updatePointers);
      window.removeEventListener('pointerup', updatePointers);
      window.removeEventListener('pointercancel', updatePointers);
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
      window.removeEventListener('blur', () => setIsWorldMode(false));
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
            
            <WindowManager />
        </div>
      </div>

      <div class="absolute bottom-6 left-6 flex flex-col gap-1 pointer-events-none z-[100]">
        <div class="text-[10px] font-black text-white/20 tracking-[0.4em] uppercase">
          JotCAD Spatial Desktop
        </div>
      </div>
    </div>
  );
};
