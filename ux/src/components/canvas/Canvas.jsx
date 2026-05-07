import {
  createSignal,
  onMount,
  onCleanup,
  For,
  createEffect,
  createMemo,
  untrack,
} from 'solid-js';
import { createStore } from 'solid-js/store';
import interact from 'interactjs';
import { blackboard, vfs } from '../../lib/blackboard';
import { setPointerCount, pointerCount } from '../../lib/state/AppState';

import { PathNode } from './PathNode';
import { AgentNode } from './AgentNode';
import { Connection } from './Connection';

import { MeshMap } from '../discovery/MeshMap';
import { CatalogNode } from '../discovery/CatalogNode';
import { Console } from '../system/Console';
import { JotNode } from '../editor/JotNode';

export const Canvas = () => {
  const [nodePositions, setNodePositions] = createStore({});
  const [view, setView] = createSignal({ x: 0, y: 0, scale: 1 });
  
  // Make view state available globally for scale-aware math in other components
  if (typeof window !== 'undefined') window._JOT_VIEW = view;

  const [windowSize, setWindowSize] = createSignal({ 
    width: window.innerWidth, 
    height: window.innerHeight,
    isMobile: window.innerWidth < 768
  });

  let canvasRef;
  let dragRef;
  let lastTouchState = { distance: 0, x: 0, y: 0 };
  let isPanning = false;

  onMount(() => {
    blackboard.start();

    const updatePointers = (e) => {
      if (!window._activePointers) window._activePointers = new Set();
      if (!window._pointerTargets) window._pointerTargets = new Map();

      if (e.type === 'pointerdown') {
        window._activePointers.add(e.pointerId);
        // Track what we touched
        const targetNode = e.target.closest('.jot-node, .path-node, .agent-node');
        window._pointerTargets.set(e.pointerId, targetNode?.dataset?.id || (targetNode ? 'some-node' : 'blackboard'));
      } else {
        window._activePointers.delete(e.pointerId);
        window._pointerTargets.delete(e.pointerId);
      }
      
      const count = window._activePointers.size;
      setPointerCount(count);

      // Ownership Logic
      if (count === 2 && !blackboard.isGesturing()) {
        const targets = Array.from(window._pointerTargets.values());
        const uniqueTargets = new Set(targets);

        blackboard.setIsGesturing(true);
        if (uniqueTargets.size === 1 && !uniqueTargets.has('blackboard')) {
          blackboard.setGestureOwner(targets[0]);
        } else {
          blackboard.setGestureOwner('blackboard');
        }

        // Prepare for new gesture sequence
        lastTouchState.distance = 0; 
      } else if (count < 2) {
        // CLOSE OUT gesture properly when fingers are lifted
        if (blackboard.isGesturing()) {
          blackboard.setIsGesturing(false);
          blackboard.setGestureOwner(null);
        }
        lastTouchState.distance = 0;
        isPanning = false; // Stop any residual 1-finger panning
      }
    };

    window.addEventListener('pointerdown', updatePointers);
    window.addEventListener('pointerup', updatePointers);
    window.addEventListener('pointercancel', updatePointers);

    const handleWheel = (e) => {
      e.preventDefault();
      // ... same logic ...
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

    const handleTouchMove = (e) => {
      if (e.touches.length === 2 && blackboard.gestureOwner() === 'blackboard') {
        e.preventDefault();
        const t1 = e.touches[0];
        const t2 = e.touches[1];
        
        const dist = Math.hypot(t1.clientX - t2.clientX, t1.clientY - t2.clientY);
        const centerX = (t1.clientX + t2.clientX) / 2;
        const centerY = (t1.clientY + t2.clientY) / 2;

        if (lastTouchState.distance === 0) {
          // INITIALIZE BASELINE and skip this frame to prevent jump
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
      if (e.target === dragRef && e.button === 0) {
        isPanning = true;
        dragRef.setPointerCapture(e.pointerId);
      }
    };

    const handlePointerMove = (e) => {
      if (isPanning && (!blackboard.gestureOwner() || blackboard.gestureOwner() === 'blackboard')) {
        setView((v) => ({ ...v, x: v.x + e.movementX, y: v.y + e.movementY }));
      }
    };

    const handlePointerUp = () => {
      isPanning = false;
    };

    canvasRef.addEventListener('wheel', handleWheel, { passive: false });
    canvasRef.addEventListener('touchmove', handleTouchMove, { passive: false });
    canvasRef.addEventListener('gesturestart', (e) => e.preventDefault(), { passive: false });
    canvasRef.addEventListener('gesturechange', (e) => e.preventDefault(), { passive: false });
    
    dragRef.addEventListener('pointerdown', handlePointerDown);
    dragRef.addEventListener('pointermove', handlePointerMove);
    dragRef.addEventListener('pointerup', handlePointerUp);
    dragRef.addEventListener('pointercancel', handlePointerUp);

    const handleResize = () => {
      setWindowSize({
        width: window.innerWidth,
        height: window.innerHeight,
        isMobile: window.innerWidth < 768
      });
    };
    window.addEventListener('resize', handleResize);

    onCleanup(() => {
      window.removeEventListener('resize', handleResize);
      window.removeEventListener('pointerdown', updatePointers);
      window.removeEventListener('pointerup', updatePointers);
      window.removeEventListener('pointercancel', updatePointers);
      canvasRef.removeEventListener('wheel', handleWheel);
      canvasRef.removeEventListener('touchmove', handleTouchMove);
      dragRef.removeEventListener('pointerdown', handlePointerDown);
      dragRef.removeEventListener('pointermove', handlePointerMove);
      dragRef.removeEventListener('pointerup', handlePointerUp);
      dragRef.removeEventListener('pointercancel', handlePointerUp);
    });
  });

  const groupedNodes = createMemo(() => {
    const nodes = blackboard.graph();
    const paths = {};
    const agents = [];

    Object.values(nodes).forEach((node) => {
      if (node.state === 'LISTENING') {
        agents.push(node);
        if (!paths[node.path]) paths[node.path] = [];
      } else if (node.state === 'SCHEMA') {
        const p = node.path.replace(/@schema$/, '');
        if (!paths[p]) paths[p] = [];
        paths[p].push(node);
      } else {
        if (!paths[node.path]) paths[node.path] = [];
        paths[node.path].push(node);
      }
    });

    return { paths, agents };
  });

  createEffect(() => {
    const { paths, agents } = groupedNodes();
    const { width, isMobile } = windowSize();

    Object.keys(paths).forEach((path, i) => {
      if (!nodePositions[path]) {
        const spacing = isMobile ? 120 : 180;
        const availableWidth = Math.max(width - (isMobile ? 100 : 450), 200);
        
        setNodePositions(path, {
          x: (isMobile ? 50 : 350) + ((i * spacing) % availableWidth),
          y: (isMobile ? 100 : 150) + Math.floor(i / (isMobile ? 2 : 4)) * spacing,
        });
      }
    });

    agents.forEach((agent, i) => {
      if (!nodePositions[agent.cid]) {
        setNodePositions(agent.cid, { 
          x: isMobile ? 40 : 120, 
          y: (isMobile ? 150 : 120) + i * (isMobile ? 100 : 120) 
        });
      }
    });
  });

  const handleMove = (id, dx, dy) => {
    const s = untrack(() => view().scale);
    setNodePositions(id, (prev) => ({
      x: (prev?.x || 0) + dx / s,
      y: (prev?.y || 0) + dy / s,
    }));
  };

  const connections = createMemo(() => {
    const { paths, agents } = groupedNodes();
    const conns = [];

    agents.forEach((agent) => {
      const agentPos = nodePositions[agent.cid];
      if (!agentPos) return;

      Object.keys(paths).forEach((path) => {
        if (path.startsWith(agent.path)) {
          const pathPos = nodePositions[path];
          if (pathPos) {
            conns.push({
              from: agentPos,
              fromSize: 56,
              to: pathPos,
              toSize: 48,
              color: '#f97316',
              opacity: 0.4,
            });
          }
        }
      });
    });

    return conns;
  });

  return (
    <div
      class="canvas-container w-full h-full overflow-hidden bg-blackboard relative select-none"
      ref={canvasRef}
    >
      {/* Pan/Grab Layer */}
      <div
        ref={dragRef}
        class="absolute inset-0 z-0 cursor-grab active:cursor-grabbing pointer-events-auto"
      />

      <div
        class="absolute inset-0 pointer-events-none"
        style={{
          transform: `translate(${view().x}px, ${view().y}px) scale(${
            view().scale
          })`,
          'transform-origin': '0 0',
        }}
      >
        <MeshMap scale={view().scale} />

        <svg class="absolute inset-0 w-full h-full pointer-events-none overflow-visible">
          <For each={connections()}>
            {(conn) => (
              <Connection
                fromPos={conn.from}
                fromSize={conn.fromSize}
                toPos={conn.to}
                toSize={conn.toSize}
                color={conn.color}
                opacity={conn.opacity}
              />
            )}
          </For>
        </svg>

        <div class="absolute inset-0 pointer-events-none overflow-visible">
          <For each={Object.keys(groupedNodes().paths)}>
            {(path) => (
              <PathNode
                path={path}
                results={groupedNodes().paths[path]}
                pos={nodePositions[path]}
                onMove={handleMove}
              />
            )}
          </For>

          <For each={groupedNodes().agents}>
            {(agent) => (
              <AgentNode
                data={agent}
                pos={nodePositions[agent.cid]}
                onMove={handleMove}
              />
            )}
          </For>

          <div class="pointer-events-auto">
            <CatalogNode />
            <For each={blackboard.openEditors()} by="id">
              {(editorState) => <JotNode initial={editorState} />}
            </For>
          </div>
        </div>
      </div>

      {/* HUD Layer */}
      <div class="absolute inset-0 z-50 pointer-events-none">
        <div class="pointer-events-auto">
          <Console />
        </div>
      </div>

      <div class="absolute bottom-4 right-4 z-[100] flex flex-col items-end gap-2 pointer-events-none">
        <div class="px-3 py-1 bg-red-600 text-white text-[10px] font-black animate-pulse rounded-full shadow-2xl">
          DEBUG: VERSION 2.2 ATOMIC
        </div>
        <div class="px-2 py-0.5 bg-black/80 text-cyan-400 text-[8px] font-mono rounded border border-cyan-400/20">
          LOGS: window._JOT_LOG
        </div>
      </div>

      <div class="absolute top-4 left-4 z-50 flex items-center gap-2 px-3 py-1.5 rounded-full bg-black/80 backdrop-blur-md border border-white/20 shadow-xl">
        <div
          class={`w-2.5 h-2.5 rounded-full ${
            blackboard.isConnected()
              ? 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]'
              : 'bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]'
          }`}
        />
        <span class="text-[10px] font-black text-white/70 tracking-widest uppercase">
          {blackboard.isConnected() ? `Mesh Online: ${blackboard.peerId}` : 'Mesh Offline'}
        </span>
      </div>

      <div class="absolute bottom-4 left-4 md:bottom-6 md:left-6 flex flex-col gap-1 pointer-events-none z-50">
        <div class="text-[8px] md:text-[10px] font-black text-white/20 tracking-[0.2em] md:tracking-[0.4em]">
          JotCAD Distributed Blackboard
        </div>
        <div class="hidden md:block text-[8px] font-mono text-white/10 tracking-widest opacity-50 uppercase text-balance">
          Double-tap Node to view results • Drag background to Pan •
          Pinch to Zoom
        </div>
      </div>
    </div>
  );
};
