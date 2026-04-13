import { For, Show, createMemo, createEffect, onMount } from 'solid-js';
import { blackboard } from '../lib/blackboard';
import { Cpu, Globe, Zap, Box, Hexagon, Triangle, FileJson } from 'lucide-solid';
import interact from 'interactjs';

export function MeshMap() {
  const topo = blackboard.meshTopology;
  const pulses = blackboard.pulse;
  const positions = blackboard.meshPositions;
  const schemas = blackboard.schemas;

  const nodeRefs = new Map();

  onMount(() => {
    // We'll manage draggability dynamically as nodes are added
  });

  createEffect(() => {
    // Ensure all current nodes are draggable
    topo().peers.forEach(node => {
        const el = nodeRefs.get(node.id);
        if (el && !el._draggable) {
            el._draggable = true;
            interact(el).draggable({
                listeners: {
                    move(event) {
                        const current = positions()[node.id] || { x: node.x || 0, y: node.y || 0 };
                        blackboard.setMeshPositions(prev => ({
                            ...prev,
                            [node.id]: { x: current.x + event.dx, y: current.y + event.dy }
                        }));
                    }
                }
            });
        }
    });
  });

  const getEnv = (id) => {
      if (id.includes('ops')) return 'C++';
      if (id.includes('export')) return 'JS';
      if (id.includes('ui')) return 'JS';
      return 'MESH';
  };

  const nodes = createMemo(() => {
    const peers = topo().peers || [];
    const centerX = window.innerWidth / 2;
    const centerY = window.innerHeight / 2;

    return peers.map((p, i) => {
        const pos = positions()[p.id];
        if (pos) return { ...p, ...pos };

        // Fallback layout
        const angle = (i / peers.length) * Math.PI * 2;
        return {
            ...p,
            x: centerX + Math.cos(angle) * 200 - (peers.length > 1 ? 100 : 0),
            y: centerY + Math.sin(angle) * 200
        };
    });
  });

  const nodeOps = (peerId) => {
      const all = schemas();
      return Object.entries(all)
        .filter(([path, schema]) => schema._origin === peerId)
        .map(([path]) => path.split('/').pop());
  };

  const edges = createMemo(() => {
    const ns = nodes();
    const result = [];
    const seen = new Set();

    ns.forEach(node => {
        (node.neighbors || []).forEach(neighbor => {
            const target = ns.find(n => n.id === neighbor.id);
            if (target) {
                const edgeKey = [node.id, target.id].sort().join('--');
                if (!seen.has(edgeKey)) {
                    seen.add(edgeKey);
                    result.push({ from: node, to: target, reachability: neighbor.reachability });
                }
            }
        });
    });
    return result;
  });

  return (
    <div class="fixed inset-0 pointer-events-none z-0 overflow-hidden">
      <svg class="w-full h-full">
        {/* Adjacency Edges */}
        <For each={edges()}>
          {(edge) => (
            <line
              x1={edge.from.x} y1={edge.from.y}
              x2={edge.to.x} y2={edge.to.y}
              stroke="white"
              stroke-width="1"
              stroke-opacity="0.1"
              stroke-dasharray={edge.reachability === 'REVERSE' ? '4 4' : '0'}
            />
          )}
        </For>

        {/* Real-time Particles */}
        <For each={pulses()}>
            {(pulse) => {
                const stack = pulse.payload?.stack || [];
                if (stack.length < 2) return null;
                const fromId = stack[stack.length - 1];
                const toId = stack[stack.length - 2];
                const fromNode = nodes().find(n => n.id === fromId);
                const toNode = nodes().find(n => n.id === toId);
                if (!fromNode || !toNode) return null;

                return (
                    <circle r="3" fill="#f97316" class="animate-ping">
                        <animateMotion
                            dur="0.4s"
                            repeatCount="1"
                            path={`M ${fromNode.x} ${fromNode.y} L ${toNode.x} ${toNode.y}`}
                        />
                    </circle>
                );
            }}
        </For>

        {/* Nodes */}
        <For each={nodes()}>
          {(node) => (
            <g 
                transform={`translate(${node.x}, ${node.y})`}
                ref={(el) => nodeRefs.set(node.id, el)}
                class="pointer-events-auto cursor-grab active:cursor-grabbing"
            >
              {/* Ops Bloom (The Ring) */}
              <For each={nodeOps(node.id)}>
                  {(op, i) => {
                      const angle = (i / nodeOps(node.id).length) * Math.PI * 2;
                      const r = 55;
                      return (
                          <g transform={`translate(${Math.cos(angle) * r}, ${Math.sin(angle) * r})`}>
                              <circle r="12" fill="black" fill-opacity="0.4" stroke="white" stroke-opacity="0.1" />
                              <text 
                                text-anchor="middle" 
                                dy="3" 
                                fill="white" 
                                fill-opacity="0.3" 
                                class="text-[6px] font-black uppercase"
                              >
                                {op.slice(0, 3)}
                              </text>
                          </g>
                      );
                  }}
              </For>

              <circle r="35" fill={node.type === 'BROWSER' ? '#3b82f6' : '#f97316'} fill-opacity={Math.min(0.4, (node.pps || 0) / 10)} />
              <circle r="25" fill="#000" fill-opacity="0.8" stroke={node.type === 'BROWSER' ? '#3b82f6' : '#f97316'} stroke-width="2" stroke-opacity="0.4" />

              <foreignObject x="-15" y="-15" width="30" height="30">
                <div class="w-full h-full flex items-center justify-center text-white/40">
                  {node.type === 'BROWSER' ? <Globe size={16} /> : <Cpu size={16} />}
                </div>
              </foreignObject>

              {/* Language Tag */}
              <g transform="translate(0, 32)">
                  <rect x="-12" y="-6" width="24" height="12" rx="3" fill={node.type === 'BROWSER' ? '#3b82f6' : '#f97316'} fill-opacity="0.2" />
                  <text text-anchor="middle" dy="3" fill="white" fill-opacity="0.6" class="text-[7px] font-black">{getEnv(node.id)}</text>
              </g>

              {/* PPS */}
              <Show when={node.pps > 0}>
                <text y="45" text-anchor="middle" fill="#f97316" class="text-[8px] font-bold font-mono">{node.pps} PPS</text>
              </Show>

              {/* Short ID */}
              <text y="-40" text-anchor="middle" fill="white" fill-opacity="0.4" class="text-[7px] font-bold uppercase tracking-widest">
                {node.id.split('-').pop()}
              </text>
            </g>
          )}
        </For>
      </svg>
    </div>
  );
}
