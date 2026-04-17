import {
  For,
  Show,
  createMemo,
  createEffect,
  onMount,
  untrack,
  onCleanup,
} from 'solid-js';
import { blackboard } from '../lib/blackboard';
import { Cpu, Globe, Zap, FileJson, Shield, Workflow } from 'lucide-solid';
import interact from 'interactjs';

const getEnvInfo = (node) => {
  const id = node.id.toLowerCase();
  if (id.includes('ops') || id.includes('cpp'))
    return { label: 'C++', icon: <Cpu size={16} />, color: '#f97316' };
  if (id.includes('service-worker'))
    return { label: 'SW', icon: <Shield size={16} />, color: '#10b981' };
  if (id.includes('web-worker') || id.includes('worker'))
    return { label: 'Worker', icon: <Workflow size={16} />, color: '#8b5cf6' };
  if (id.includes('node'))
    return { label: 'Node', icon: <Zap size={16} />, color: '#eab308' };
  if (node.type === 'BROWSER')
    return { label: 'Browser', icon: <Globe size={16} />, color: '#3b82f6' };
  return { label: 'JS', icon: <FileJson size={16} />, color: '#64748b' };
};

const MeshNode = (props) => {
  let nodeRef;
  const env = createMemo(() => getEnvInfo(props.node));

  const pos = createMemo(() => {
    const p = props.node;
    const saved = blackboard.meshPositions()[p.id];
    if (saved) return saved;

    const centerX = 400; // Consistent with force layout
    const centerY = 500;
    const angle = (props.index / Math.max(1, props.total)) * Math.PI * 2;
    return {
      x: centerX + Math.cos(angle) * 100,
      y: centerY + Math.sin(angle) * 100,
    };
  });

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          const s = props.scale || 1;
          const current = untrack(pos);
          blackboard.setMeshPositions((prev) => ({
            ...prev,
            [props.node.id]: {
              x: current.x + event.dx / s,
              y: current.y + event.dy / s,
              manual: true,
            },
          }));
        },
      },
    });
  });

  return (
    <div
      ref={nodeRef}
      class="absolute pointer-events-auto cursor-grab active:cursor-grabbing flex items-center justify-center select-none w-12 h-12"
      style={{
        left: `${pos().x - 24}px`,
        top: `${pos().y - 24}px`,
        'z-index': 100,
      }}
      title={`ID: ${props.node.id}`}
    >
      <div
        class="absolute w-16 h-16 rounded-full animate-ping opacity-10"
        style={{ 'background-color': env().color }}
      />
      <div
        class="absolute w-12 h-12 rounded-full border-4 flex items-center justify-center bg-black shadow-2xl z-10"
        style={{
          'border-color': env().color,
          'box-shadow': `0 0 20px ${env().color}44`,
        }}
      >
        <div style={{ color: env().color }}>{env().icon}</div>
      </div>

      <Show when={props.node.pps > 0}>
        <div
          class="absolute top-14 whitespace-nowrap text-[9px] font-black font-mono animate-pulse"
          style={{ color: env().color }}
        >
          {props.node.pps} PPS
        </div>
      </Show>
    </div>
  );
};

export function MeshMap(props) {
  const topo = blackboard.meshTopology;
  const pulses = blackboard.pulse;
  const positions = blackboard.meshPositions;

  const OP_LIST_WIDTH = 280;
  const OP_LIST_RIGHT = 24;
  // Global Operators list
  const allOps = createMemo(() => {
    const all = blackboard.schemas();
    return Object.entries(all)
      .filter(([path]) => path.startsWith('jot/')) // Only show jot/ operators in the list
      .map(([path, schema]) => ({
        id: path,
        // Remove jot/ and make uppercase
        name: path.replace(/^jot\//, '').toUpperCase(),
        origin: (schema._origin || 'unknown').toLowerCase(),
        path: path.toLowerCase(),
      }))
      .sort((a, b) => a.path.localeCompare(b.path));
  });

  // Tight Force Layout (Stable Loop)
  onMount(() => {
    const interval = setInterval(() => {
      const peers = untrack(topo).peers || [];
      if (peers.length < 2) return;

      blackboard.setMeshPositions((prev) => {
        const next = { ...prev };

        for (let i = 0; i < peers.length; i++) {
          const p1 = peers[i];
          if (!next[p1.id])
            next[p1.id] = { x: 400 + Math.random(), y: 500 + Math.random() };
          if (next[p1.id].manual) continue;

          for (let j = i + 1; j < peers.length; j++) {
            const p2 = peers[j];
            if (!next[p2.id])
              next[p2.id] = { x: 400 + Math.random(), y: 500 + Math.random() };

            const dx = next[p1.id].x - next[p2.id].x;
            const dy = next[p1.id].y - next[p2.id].y;
            const distSq = dx * dx + dy * dy;
            const dist = Math.sqrt(distSq) || 1;

            // Strong repulsion
            const force = 10000 / distSq;
            const fx = (dx / dist) * force;
            const fy = (dy / dist) * force;

            if (!next[p1.id].manual) {
              next[p1.id].x += fx;
              next[p1.id].y += fy;
            }
            if (!next[p2.id].manual) {
              next[p2.id].x -= fx;
              next[p2.id].y -= fy;
            }
          }

          const dcx = 400 - next[p1.id].x;
          const dcy = 500 - next[p1.id].y;
          next[p1.id].x += dcx * 0.1;
          next[p1.id].y += dcy * 0.1;
        }
        return next;
      });
    }, 16);
    onCleanup(() => clearInterval(interval));
  });

  const nodes = createMemo(() => {
    const t = topo();
    const peers = t.peers || [];
    return peers.map((p) => {
      const pos = positions()[p.id] || { x: 400, y: 500 };
      return { ...p, x: pos.x, y: pos.y };
    });
  });

  const hostedLines = createMemo(() => {
    const ns = nodes();
    const ops = untrack(allOps);
    const lines = [];

    ns.forEach((node) => {
      const pId = node.id.toLowerCase();
      const env = getEnvInfo(node);

      ops.forEach((op, idx) => {
        let hosted = false;
        const origin = op.origin;
        if (origin !== 'unknown' && origin === pId) hosted = true;
        else if (
          origin !== 'unknown' &&
          (pId.includes(origin) || origin.includes(pId))
        )
          hosted = true;
        else {
          const parts = op.path.split('/');
          for (const part of parts) {
            if (part.length > 2 && pId.includes(part)) {
              hosted = true;
              break;
            }
          }
        }

        if (hosted) {
          lines.push({
            x1: node.x,
            y1: node.y,
            // Calculate precisely based on fixed layout
            x2: window.innerWidth - OP_LIST_RIGHT - OP_LIST_WIDTH,
            y2: 52 + idx * 22,
            color: env.color,
          });
        }
      });
    });
    return lines;
  });

  const topoEdges = createMemo(() => {
    const ns = nodes();
    const result = [];
    const seen = new Set();

    ns.forEach((node) => {
      (node.neighbors || []).forEach((neighbor) => {
        const target = ns.find((n) => n.id === neighbor.id);
        if (target) {
          const edgeKey = [node.id, target.id].sort().join('--');
          if (!seen.has(edgeKey)) {
            seen.add(edgeKey);
            result.push({
              from: node,
              to: target,
              reachability: neighbor.reachability,
            });
          }
        }
      });
    });
    return result;
  });

  return (
    <div class="absolute inset-0 pointer-events-none z-0 overflow-hidden">
      <svg class="w-full h-full overflow-visible">
        {/* Connection lines from nodes to operators */}
        <For each={hostedLines()}>
          {(line) => (
            <line
              x1={line.x1}
              y1={line.y1}
              x2={line.x2}
              y2={line.y2}
              stroke={line.color}
              stroke-width="1.5"
              stroke-opacity="0.2"
              stroke-dasharray="2 2"
            />
          )}
        </For>

        {/* Peer connections */}
        <For each={topoEdges()}>
          {(edge) => (
            <line
              x1={edge.from.x}
              y1={edge.from.y}
              x2={edge.to.x}
              y2={edge.to.y}
              stroke="white"
              stroke-width="1"
              stroke-opacity="0.1"
              stroke-dasharray={edge.reachability === 'REVERSE' ? '4 4' : '0'}
            />
          )}
        </For>

        {/* Pulse bubbles */}
        <For each={pulses()}>
          {(pulse) => {
            const stack = pulse.payload?.stack || [];
            if (stack.length < 2) return null;
            const fromId = stack[stack.length - 1];
            const toId = stack[stack.length - 2];
            const fromNode = nodes().find((n) => n.id === fromId);
            const toNode = nodes().find((n) => n.id === toId);
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
      </svg>

      {/* Fixed Operator List on Right (Left Justified) */}
      <div
        class="absolute top-10 py-2 flex flex-col gap-[2px] pointer-events-none items-start"
        style={{
          right: `${OP_LIST_RIGHT}px`,
          width: `${OP_LIST_WIDTH}px`,
        }}
      >
        <For each={allOps()}>
          {(op, idx) => (
            <div class="h-[20px] flex items-center gap-2">
              {/* Anchor dot on the LEFT of the name */}
              <div
                class="w-1.5 h-1.5 rounded-full flex-shrink-0"
                style={{
                  'background-color':
                    hostedLines().find((l) => l.y2 === 52 + idx() * 22)
                      ?.color || '#333',
                }}
              />
              <span class="text-[10px] font-black uppercase tracking-widest text-white/60 whitespace-nowrap">
                {op.name}
              </span>
            </div>
          )}
        </For>
      </div>

      <div class="absolute inset-0 pointer-events-none">
        <For each={topo().peers}>
          {(node, idx) => (
            <MeshNode
              node={node}
              index={idx()}
              total={topo().peers.length}
              scale={props.scale}
            />
          )}
        </For>
      </div>
    </div>
  );
}
