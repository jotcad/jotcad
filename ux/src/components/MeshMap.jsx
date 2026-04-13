import { For, Show, createMemo, createEffect, createSignal } from 'solid-js';
import { blackboard } from '../lib/blackboard';
import { Cpu, Globe, Zap } from 'lucide-solid';

export function MeshMap() {
  const topo = blackboard.meshTopology;
  const pulses = blackboard.pulse;

  // Simple fixed layout for common multi-hop:
  // Browser (Center) <-> Export (Right) <-> Ops (Far Right)
  const nodePositions = {
      'ui-main': { x: window.innerWidth / 2 - 200, y: window.innerHeight / 2 },
      'pipeline-export': { x: window.innerWidth / 2, y: window.innerHeight / 2 },
      'pipeline-ops': { x: window.innerWidth / 2 + 200, y: window.innerHeight / 2 }
  };

  const nodes = createMemo(() => {
    const peers = topo().peers || [];
    const centerX = window.innerWidth / 2;
    const centerY = window.innerHeight / 2;

    return peers.map((p, i) => {
        // Use fixed pos if known, else circle
        const fixed = nodePositions[p.id] || Object.values(nodePositions).find(pos => p.id.includes(pos.id));
        if (fixed) return { ...p, ...fixed };

        const angle = (i / peers.length) * Math.PI * 2;
        return {
            ...p,
            x: centerX + Math.cos(angle) * 150,
            y: centerY + Math.sin(angle) * 150
        };
    });
  });

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
                    result.push({
                        from: node,
                        to: target,
                        reachability: neighbor.reachability
                    });
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
            <g>
              <line
                x1={edge.from.x} y1={edge.from.y}
                x2={edge.to.x} y2={edge.to.y}
                stroke="white"
                stroke-width="1"
                stroke-opacity="0.1"
                stroke-dasharray={edge.reachability === 'REVERSE' ? '4 4' : '0'}
              />
            </g>
          )}
        </For>

        {/* Real-time Particles (Animated based on Pulse Stack) */}
        <For each={pulses()}>
            {(pulse) => {
                const stack = pulse.payload.stack || [];
                if (stack.length < 2) return null;
                
                // Animate from the last hop back to the browser
                const fromId = stack[stack.length - 1];
                const toId = stack[stack.length - 2] || 'ui-main';
                
                const fromNode = nodes().find(n => n.id === fromId);
                const toNode = nodes().find(n => n.id === toId || (toId === 'ui-main' && n.type === 'BROWSER'));

                if (!fromNode || !toNode) return null;

                return (
                    <circle r="3" fill="#f97316" class="animate-ping">
                        <animateMotion
                            dur="0.5s"
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
            <g transform={`translate(${node.x}, ${node.y})`}>
              <circle
                r="35"
                fill={node.type === 'BROWSER' ? '#3b82f6' : '#f97316'}
                fill-opacity={Math.min(0.4, (node.pps || 0) / 10)}
                class="transition-all duration-500"
              />
              
              <circle
                r="25"
                fill="#000"
                fill-opacity="0.8"
                stroke={node.type === 'BROWSER' ? '#3b82f6' : '#f97316'}
                stroke-width="2"
                stroke-opacity="0.4"
              />

              <foreignObject x="-15" y="-15" width="30" height="30">
                <div class="w-full h-full flex items-center justify-center text-white/40">
                  {node.type === 'BROWSER' ? <Globe size={16} /> : <Cpu size={16} />}
                </div>
              </foreignObject>

              <Show when={node.pps > 0}>
                <g transform="translate(0, 40)">
                  <text text-anchor="middle" fill="#f97316" class="text-[8px] font-bold font-mono">
                    {node.pps} PPS
                  </text>
                </g>
              </Show>

              <text
                y="-35"
                text-anchor="middle"
                fill="white"
                fill-opacity="0.4"
                class="text-[7px] font-bold uppercase tracking-widest"
              >
                {node.id.split('-').pop()}
              </text>
            </g>
          )}
        </For>
      </svg>
    </div>
  );
}
