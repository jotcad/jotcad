import { createSignal, onMount, onCleanup, For, Show, createEffect } from 'solid-js';
import interact from 'interactjs';
import { blackboard, vfs } from '../lib/blackboard';
import { initSharedRenderer, createScene, updateViewports } from '../lib/three_utils';
import { ScriptNode } from './ScriptNode';
import * as THREE from 'three';

/**
 * A draggable coordinate node.
 */
const Node = (props) => {
  let nodeRef;
  let previewRef;

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          props.onMove(props.data.cid, event.dx, event.dy);
        },
      },
    });

    if (props.data.state === 'AVAILABLE') {
        // Setup Three.js scene for this node
        // (Implementation details for actual mesh rendering omitted for brevity)
    }
  });

  const stateColor = () => {
    switch (props.data.state) {
      case 'AVAILABLE': return 'bg-available/20 border-available';
      case 'PROVISIONING': return 'bg-provisioning/20 border-provisioning animate-pulse';
      case 'PENDING': return 'bg-pending/20 border-pending';
      case 'LINKED': return 'bg-white/10 border-white/30 border-dashed';
      default: return 'bg-node border-wire';
    }
  };

  return (
    <div
      ref={nodeRef}
      class={`absolute select-none cursor-move p-4 rounded-lg border-2 shadow-xl backdrop-blur-md transition-colors duration-500 ${stateColor()}`}
      style={{
        transform: `translate(${props.pos.x}px, ${props.pos.y}px)`,
        width: '240px',
        'z-index': 10
      }}
    >
      <div class="flex flex-col gap-2">
        <div class="text-xs font-bold uppercase opacity-50 tracking-widest flex justify-between">
            <span>{props.data.path}</span>
            <Show when={props.data.source}>
                <span class="text-[8px] px-1 bg-white/10 rounded">{props.data.source}</span>
            </Show>
        </div>
        <div class="text-[8px] font-mono opacity-20 truncate">{props.data.cid}</div>
        <div class="text-sm font-mono truncate bg-black/20 p-1 rounded border border-white/5">
            {JSON.stringify(props.data.parameters)}
        </div>
        
        <Show when={props.data.state === 'AVAILABLE'}>
            <div 
                ref={previewRef}
                class="mt-2 h-32 bg-black/40 rounded border border-white/5 flex items-center justify-center overflow-hidden relative"
            >
                <div class="absolute inset-0 flex items-center justify-center text-[10px] opacity-20 italic">
                    {props.data.path.includes('mesh') ? 'MESH DATA' : 'JSON OBJECT'}
                </div>
            </div>
        </Show>

        <Show when={props.data.state === 'LINKED'}>
            <div class="text-[10px] italic opacity-50 mt-1 flex flex-col gap-1">
                <span class="opacity-30 underline decoration-dotted">Alias for:</span>
                <span class="truncate font-mono bg-white/5 p-1 rounded">{props.data.target.path}</span>
            </div>
        </Show>

        <div class="flex justify-between items-center mt-2 pt-2 border-t border-white/5">
            <span class={`text-[10px] font-bold ${props.data.state === 'AVAILABLE' ? 'text-available' : 'opacity-50'}`}>
                {props.data.state}
            </span>
            <button 
                class="px-2 py-1 bg-white/5 hover:bg-white/10 active:scale-95 transition-transform rounded text-[10px]"
                onClick={() => blackboard.tickle(props.data.path, props.data.parameters)}
            >
                REFRESH
            </button>
        </div>
      </div>
    </div>
  );
};

/**
 * The infinite canvas for the blackboard graph.
 */
export const Canvas = () => {
  const [nodePositions, setNodePositions] = createSignal({});
  let canvasRef;

  onMount(() => {
    blackboard.start();
    const renderer = initSharedRenderer();
    canvasRef.appendChild(renderer.domElement);
    
    const animate = () => {
        updateViewports();
        requestAnimationFrame(animate);
    };
    animate();
  });

  createEffect(() => {
    const nodes = blackboard.graph();
    const currentPositions = { ...nodePositions() };
    let changed = false;

    Object.values(nodes).forEach((node) => {
      if (!currentPositions[node.cid]) {
        currentPositions[node.cid] = { 
            x: 100 + (Object.keys(currentPositions).length * 280) % (window.innerWidth - 300), 
            y: 150 + Math.floor(Object.keys(currentPositions).length / 4) * 240 
        };
        changed = true;
      }
    });

    if (changed) setNodePositions(currentPositions);
  });

  const handleMove = (cid, dx, dy) => {
    setNodePositions(prev => ({
      ...prev,
      [cid]: { x: prev[cid].x + dx, y: prev[cid].y + dy }
    }));
  };

  onCleanup(() => {
    blackboard.stop();
  });

  return (
    <div class="canvas-container w-full h-full overflow-hidden bg-blackboard relative" ref={canvasRef}>
      {/* 3D layer is managed by three_utils */}
      
      <svg class="absolute inset-0 w-full h-full pointer-events-none z-0">
        <For each={Object.values(blackboard.graph())}>
            {(node) => (
                <Show when={node.state === 'LINKED'}>
                    {(() => {
                        // Find target CID if possible
                        // This would need a way to look up CID from target path/params
                        return null;
                    })()}
                </Show>
            )}
        </For>
      </svg>
      
      <div class="absolute inset-0 z-10 overflow-hidden pointer-events-none">
        <div class="pointer-events-auto">
            <ScriptNode />
        </div>
        <For each={Object.values(blackboard.graph())}>
            {(node) => (
            <div class="pointer-events-auto">
                <Node 
                    data={node} 
                    pos={nodePositions()[node.cid] || { x: 0, y: 0 }}
                    onMove={handleMove}
                />
            </div>
            )}
        </For>
      </div>

      {/* UI Controls */}
      <div class="absolute top-4 left-4 flex gap-2 z-50 p-1 bg-black/60 backdrop-blur-2xl rounded-xl border border-white/10 shadow-2xl">
        <input 
            type="text" 
            placeholder="coordinate/path (e.g. shape/box)"
            class="bg-transparent px-4 py-2 text-sm focus:outline-none w-80 text-white placeholder-white/20"
            id="path-input"
            onKeyDown={(e) => {
                if (e.key === 'Enter') {
                    blackboard.tickle(e.target.value);
                    e.target.value = '';
                }
            }}
        />
        <button 
            class="bg-available hover:bg-available/80 text-black px-6 py-2 rounded-lg text-sm font-bold transition-all active:scale-95 shadow-lg shadow-available/20"
            onClick={() => {
                const el = document.getElementById('path-input');
                blackboard.tickle(el.value);
                el.value = '';
            }}
        >
            DEMAND
        </button>
      </div>

      <div class="absolute bottom-4 right-4 flex flex-col items-end gap-1 pointer-events-none">
        <div class="text-[10px] opacity-20 font-mono tracking-tighter">JOTCAD BLACKBOARD V1 // DISTRIBUTED IDEMPOTENT FS</div>
        <div class="text-[8px] opacity-10 font-mono">2026 XYFLOW + SOLID + THREE</div>
      </div>
    </div>
  );
};
