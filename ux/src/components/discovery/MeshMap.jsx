import {
  For,
  Show,
  createMemo,
} from 'solid-js';
import { blackboard } from '../../lib/blackboard';
import { Cpu, Globe, Zap, FileJson, Shield, Workflow } from 'lucide-solid';

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
  const env = createMemo(() => getEnvInfo(props.node));

  const tooltip = createMemo(() => {
    const lines = [`ID: ${props.node.id}`];
    if (props.node.interests && props.node.interests.length > 0) {
      lines.push('\nInterests:');
      props.node.interests.forEach(i => {
        lines.push(` - ${i.path} (${i.local ? 'local' : 'remote'})`);
      });
    }
    return lines.join('\n');
  });

  return (
    <div
      class="absolute pointer-events-auto flex items-center justify-center select-none w-12 h-12"
      style={{
        left: `${props.node.x - 24}px`,
        top: `${props.node.y - 24}px`,
        'z-index': 100,
      }}
      title={tooltip()}
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
  const localId = blackboard.peerId;

  const nodes = createMemo(() => {
    const t = topo();
    const peers = t?.peers || [];
    const localNodeId = localId;

    // 1. BFS to assign levels (Sugiyama-style layout)
    const levels = new Map();
    const queue = [{ id: localNodeId, depth: 0 }];
    levels.set(localNodeId, 0);

    const processed = new Set();
    while (queue.length > 0) {
      const { id, depth } = queue.shift();
      processed.add(id);

      const peer = peers.find(p => p.id === id);
      if (peer && peer.neighbors) {
        for (const neighbor of peer.neighbors) {
          if (!levels.has(neighbor.id)) {
            levels.set(neighbor.id, depth + 1);
            queue.push({ id: neighbor.id, depth: depth + 1 });
          }
        }
      }
    }

    // Handle unreachable nodes (Level -1 or orphans)
    peers.forEach(p => { if (!levels.has(p.id)) levels.set(p.id, 99); });

    // 2. Group nodes by level for horizontal spacing
    const levelGroups = new Map();
    peers.forEach(p => {
      const lv = levels.get(p.id);
      if (!levelGroups.has(lv)) levelGroups.set(lv, []);
      levelGroups.get(lv).push(p);
    });

    const cX = props.isWindowed ? 250 : 400;
    const levelHeight = props.isWindowed ? 120 : 180;
    const nodeWidth = props.isWindowed ? 100 : 150;

    return peers.map((p) => {
      const lv = levels.get(p.id);
      const group = levelGroups.get(lv);
      const idx = group.indexOf(p);
      const totalInLevel = group.length;

      // Horizontal offset to center the level
      const rowWidth = (totalInLevel - 1) * nodeWidth;
      const startX = cX - rowWidth / 2;

      return {
        ...p,
        x: startX + idx * nodeWidth,
        y: 80 + lv * levelHeight,
        isLocal: p.id === localNodeId
      };
    });
  });

  const topoEdges = createMemo(() => {
    const ns = nodes() || [];
    const result = [];
    const seen = new Set();

    ns.forEach((node) => {
      if (!node) return;
      (node.neighbors || []).forEach((neighbor) => {
        if (!neighbor) return;
        const target = ns.find((n) => n && n.id === neighbor.id);
        if (target) {
          const edgeKey = [node.id, target.id].sort().join('--');
          if (!seen.has(edgeKey)) {
            seen.add(edgeKey);
            
            const isReverse = neighbor.reachability === 'REVERSE';
            
            result.push({
              from: isReverse ? target : node,
              to: isReverse ? node : target,
              reachability: neighbor.reachability,
              protocol: neighbor.protocol,
              originalFrom: node.id,
              originalTo: target.id
            });
          }
        }
      });
    });
    return result;
  });

  return (
    <div class={`absolute inset-0 pointer-events-none z-0 overflow-visible ${props.isWindowed ? 'bg-slate-950/50' : ''}`}>
      <svg class="w-full h-full overflow-visible">
        <defs>
          <marker id="arrow-http" viewBox="0 0 10 10" refX="25" refY="5" markerWidth="4" markerHeight="4" orient="auto-start-reverse">
            <path d="M 0 0 L 10 5 L 0 10 z" fill="#06b6d4" />
          </marker>
          <marker id="arrow-ws" viewBox="0 0 10 10" refX="25" refY="5" markerWidth="4" markerHeight="4" orient="auto-start-reverse">
            <path d="M 0 0 L 10 5 L 0 10 z" fill="#a855f7" />
          </marker>
        </defs>

        <For each={topoEdges()}>
          {(edge) => (
            <line
              x1={edge.from.x}
              y1={edge.from.y}
              x2={edge.to.x}
              y2={edge.to.y}
              stroke={edge.protocol === 'WS' ? "#a855f7" : "#06b6d4"}
              stroke-width="2"
              stroke-opacity="0.6"
              stroke-dasharray={edge.reachability === 'REVERSE' ? '5 5' : '0'}
              marker-end={edge.protocol === 'WS' ? 'url(#arrow-ws)' : 'url(#arrow-http)'}
              class="pointer-events-auto cursor-help"
            >
              <title>{`${edge.originalFrom} -> ${edge.originalTo}\nProtocol: ${edge.protocol}\nReachability: ${edge.reachability}`}</title>
            </line>
          )}
        </For>

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

      <div class="absolute inset-0 pointer-events-none">
        <For each={nodes()}>
          {(node, idx) => (
            <MeshNode
              node={node}
              index={idx()}
              total={nodes().length}
              scale={props.scale || 1}
              isWindowed={props.isWindowed}
            />
          )}
        </For>
      </div>

      <Show when={!props.isWindowed}>
          <div class="absolute bottom-4 left-4 text-[10px] font-black text-white/20 uppercase tracking-widest">
            Mesh Topology Monitor
          </div>
      </Show>
    </div>
  );
}
