import { createSignal, onMount, onCleanup, For, Show, createEffect, createMemo } from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';
import interact from 'interactjs';
import { blackboard, vfs } from '../lib/blackboard';
import { initSharedRenderer, createScene, updateViewports } from '../lib/three_utils';
import { ScriptNode } from './ScriptNode';
import * as THREE from 'three';
import { 
  Box, 
  Triangle, 
  Hexagon as HexagonIcon, 
  Link as LinkIcon, 
  Clock, 
  Activity, 
  CheckCircle2, 
  AlertCircle,
  Eye,
  FileJson,
  Cpu,
  Layers,
  Search,
  Check
} from 'lucide-solid';

/**
 * A small status indicator pip.
 */
const StatusBadge = (props) => {
    return (
        <div class="absolute -top-1 -right-1 flex gap-0.5">
            <Show when={props.states.includes('AVAILABLE')}>
                <div class="w-3.5 h-3.5 rounded-full bg-white border border-node flex items-center justify-center shadow-lg">
                    <Check size={10} class="text-node stroke-[4]" />
                </div>
            </Show>
            <Show when={props.states.includes('PROVISIONING')}>
                <div class="w-3.5 h-3.5 rounded-full bg-provisioning border border-blackboard animate-pulse shadow-lg" />
            </Show>
            <Show when={props.states.includes('PENDING')}>
                <div class="w-3.5 h-3.5 rounded-full bg-pending border border-blackboard animate-bounce shadow-lg" />
            </Show>
        </div>
    );
};

/**
 * A draggable Path Node (Circular Resource Container).
 */
const PathNode = (props) => {
  let nodeRef;
  const [isExpanded, setIsExpanded] = createSignal(false);

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          props.onMove(props.path, event.dx, event.dy);
        },
      },
    });
  });

  const TypeIcon = () => {
    const p = props.path.toLowerCase();
    const size = isExpanded() ? 20 : 18;
    if (p.includes('box')) return <Box size={size} />;
    if (p.includes('triangle')) return <Triangle size={size} />;
    if (p.includes('mesh')) return <HexagonIcon size={size} />;
    return <FileJson size={size} />;
  };

  const currentStates = () => props.results.map(r => r.state);

  return (
    <div
      ref={nodeRef}
      class={`absolute select-none cursor-move rounded-full border-2 border-white/20 shadow-2xl flex items-center justify-center bg-node text-blackboard ${isExpanded() ? 'w-80 h-auto rounded-xl p-4 flex-col text-white bg-blackboard' : 'w-12 h-12'}`}
      style={{
        left: '0px',
        top: '0px',
        transform: `translate(${props.pos?.x || 0}px, ${props.pos?.y || 0}px)`,
        'z-index': isExpanded() ? 100 : 10,
        'pointer-events': 'auto'
      }}
      onDblClick={() => setIsExpanded(!isExpanded())}
    >
        <Show when={!isExpanded()}>
            <div class="relative flex flex-col items-center justify-center w-full h-full">
                <StatusBadge states={currentStates()} />
                <TypeIcon />
                <div class="absolute top-full mt-2 px-1.5 py-0.5 rounded text-[7px] font-black tracking-tighter whitespace-nowrap bg-black/80 backdrop-blur-sm border border-white/10 text-white shadow-xl">
                    {props.path}
                    <Show when={props.results.length > 0}>
                        <span class="ml-1 opacity-50">[{props.results.length}]</span>
                    </Show>
                </div>
            </div>
        </Show>

        <Show when={isExpanded()}>
            <div class="flex flex-col gap-3 w-full">
                <div class="flex items-center justify-between border-b border-white/10 pb-2">
                    <div class="flex items-center gap-2">
                        <Layers size={16} class="opacity-50 text-available" />
                        <span class="text-xs font-black tracking-widest truncate">{props.path}</span>
                    </div>
                    <button class="text-[10px] font-bold opacity-40 hover:opacity-100 hover:text-red-400 transition-colors" onClick={() => setIsExpanded(false)}>CLOSE</button>
                </div>

                <div class="flex flex-col gap-2 max-h-96 overflow-y-auto pr-1 custom-scrollbar">
                    <For each={props.results}>
                        {(res) => (
                            <div class="bg-black/40 rounded-lg p-2 border border-white/5 flex flex-col gap-1.5 hover:border-white/20 transition-colors">
                                <div class="flex justify-between items-center">
                                    <span class={`text-[8px] font-black tracking-widest ${res.state === 'AVAILABLE' ? 'text-available' : 'text-white/40'}`}>
                                        {res.state}
                                    </span>
                                    <span class="text-[7px] opacity-20 font-mono">{res.cid.slice(0, 8)}</span>
                                </div>
                                <div class="text-[9px] font-mono text-white/70 break-all bg-black/20 p-1.5 rounded">
                                    {JSON.stringify(res.parameters)}
                                </div>
                                <div class="flex justify-between items-center mt-1">
                                    <div class="flex items-center gap-1">
                                        <Show when={res.source}>
                                            <span class="text-[7px] px-1 bg-white/5 rounded text-white/30">{res.source}</span>
                                        </Show>
                                    </div>
                                    <button 
                                        class="px-2 py-0.5 bg-white/5 hover:bg-available hover:text-black rounded text-[8px] font-bold transition-all"
                                        onClick={() => blackboard.tickle(props.path, res.parameters)}
                                    >
                                        REFRESH
                                    </button>
                                </div>
                            </div>
                        )}
                    </For>
                    <Show when={props.results.length === 0}>
                        <div class="py-8 flex flex-col items-center gap-2 opacity-20 italic">
                            <Clock size={24} />
                            <span class="text-[10px]">No data at this path yet.</span>
                        </div>
                    </Show>
                </div>
            </div>
        </Show>
    </div>
  );
};

/**
 * An Agent bubble (Hexagonal Worker).
 */
const AgentNode = (props) => {
    let nodeRef;
    onMount(() => {
        interact(nodeRef).draggable({
            listeners: {
                move(event) {
                    props.onMove(props.data.cid, event.dx, event.dy);
                },
            },
        });
    });

    return (
        <div 
            ref={nodeRef}
            class={`absolute w-14 h-14 flex items-center justify-center cursor-move z-20 group`}
            style={{
                left: '0px',
                top: '0px',
                transform: `translate(${props.pos?.x || 0}px, ${props.pos?.y || 0}px)`,
                'pointer-events': 'auto'
            }}
        >
            <svg viewBox="0 0 100 100" class="absolute inset-0 w-full h-full drop-shadow-2xl">
                <polygon 
                    points="28,8 72,8 94,50 72,92 28,92 6,50" 
                    fill="#f97316" 
                    stroke="rgba(255,255,255,0.2)" 
                    stroke-width="4"
                    class="group-hover:fill-orange-400 transition-colors"
                />
            </svg>
            
            <Cpu size={20} class="text-blackboard relative z-10" />
            
            <div class="absolute top-full mt-2 flex flex-col items-center gap-0.5 z-10 pointer-events-none">
                <div class="px-1.5 py-0.5 rounded text-[7px] font-black bg-black/80 backdrop-blur-sm text-capability whitespace-nowrap border border-capability/20 shadow-lg">
                    {props.data.path}
                </div>
                <Show when={props.data.source}>
                    <div class="text-[5px] opacity-40 font-mono tracking-tighter text-white uppercase">{props.data.source}</div>
                </Show>
            </div>
        </div>
    );
};

/**
 * A reactive connection line.
 */
const Connection = (props) => {
    return (
        <line 
            x1={props.fromPos?.x + (props.fromSize/2)} 
            y1={props.fromPos?.y + (props.fromSize/2)} 
            x2={props.toPos?.x + (props.toSize/2)} 
            y2={props.toPos?.y + (props.toSize/2)} 
            stroke={props.color}
            stroke-width="2"
            stroke-opacity={props.opacity}
            stroke-dasharray={props.dashed ? "4 4" : ""}
        />
    );
};

export const Canvas = () => {
  const [nodePositions, setNodePositions] = createStore({});
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

  const groupedNodes = createMemo(() => {
    const nodes = blackboard.graph();
    const paths = {};
    const agents = [];

    Object.values(nodes).forEach(node => {
        if (node.state === 'LISTENING') {
            agents.push(node);
            if (!paths[node.path]) paths[node.path] = [];
        } else {
            if (!paths[node.path]) paths[node.path] = [];
            paths[node.path].push(node);
        }
    });

    return { paths, agents };
  });

  createEffect(() => {
    const { paths, agents } = groupedNodes();
    
    Object.keys(paths).forEach((path, i) => {
      if (!nodePositions[path]) {
        setNodePositions(path, { 
            x: 350 + (i * 180) % (window.innerWidth - 450), 
            y: 150 + Math.floor(i / 4) * 180 
        });
      }
    });

    agents.forEach((agent, i) => {
        if (!nodePositions[agent.cid]) {
            setNodePositions(agent.cid, { x: 120, y: 120 + (i * 120) });
        }
    });
  });

  const handleMove = (id, dx, dy) => {
    setNodePositions(id, 'x', (x) => x + dx);
    setNodePositions(id, 'y', (y) => y + dy);
  };

  const connections = createMemo(() => {
    const { paths, agents } = groupedNodes();
    const conns = [];

    agents.forEach(agent => {
        const agentPos = nodePositions[agent.cid];
        if (!agentPos) return;

        Object.keys(paths).forEach(path => {
            if (path.startsWith(agent.path)) {
                const pathPos = nodePositions[path];
                if (pathPos) {
                    conns.push({ from: agentPos, fromSize: 56, to: pathPos, toSize: 48, color: '#f97316', opacity: 0.4 });
                }
            }
        });
    });

    return conns;
  });

  return (
    <div class="canvas-container w-full h-full overflow-hidden bg-blackboard relative" ref={canvasRef}>
      <svg class="absolute inset-0 w-full h-full pointer-events-none z-0">
        <For each={connections()}>
            {(conn) => (
                <Connection 
                    fromPos={conn.from} fromSize={conn.fromSize}
                    toPos={conn.to} toSize={conn.toSize}
                    color={conn.color} opacity={conn.opacity}
                />
            )}
        </For>
      </svg>

      <div class="absolute inset-0 z-10 pointer-events-none overflow-hidden">
        <div class="pointer-events-auto">
            <ScriptNode />
        </div>
        
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
      </div>

      <div class="absolute top-4 left-4 flex gap-2 z-50 p-1 bg-black/60 backdrop-blur-2xl rounded-xl border border-white/10 shadow-2xl pointer-events-auto">
        <div class="flex items-center pl-3 text-white/30"><Search size={14} /></div>
        <input 
            type="text" 
            placeholder="coordinate/path (e.g. shape/box)"
            class="bg-transparent px-4 py-2 text-xs font-black tracking-widest focus:outline-none w-80 text-white placeholder-white/20"
            id="path-input"
            onKeyDown={(e) => {
                if (e.key === 'Enter') {
                    blackboard.tickle(e.currentTarget.value);
                    e.currentTarget.value = '';
                }
            }}
        />
        <button 
            class="bg-available hover:bg-available/80 text-black px-6 py-2 rounded-lg text-xs font-black tracking-widest transition-all active:scale-95 shadow-lg shadow-available/20"
            onClick={() => {
                const el = document.getElementById('path-input');
                blackboard.tickle(el.value);
                el.value = '';
            }}
        >
            DEMAND
        </button>
      </div>

      <div class="absolute bottom-6 left-6 flex flex-col gap-1 pointer-events-none">
        <div class="text-[10px] font-black text-white/20 tracking-[0.4em]">JotCAD Distributed Blackboard</div>
        <div class="text-[8px] font-mono text-white/10 tracking-widest opacity-50 uppercase">Double-click Path [Circle] to view results • Orange Hexagons = Agents</div>
      </div>
    </div>
  );
};
