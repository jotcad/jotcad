import { createMemo, For, createEffect, onMount, untrack } from 'solid-js';
import { createStore } from 'solid-js/store';
import { blackboard } from '../../lib/blackboard';
import { MeshMap } from './MeshMap';
import { PathNode } from '../canvas/PathNode';
import { AgentNode } from '../canvas/AgentNode';
import { Connection } from '../canvas/Connection';

export const MeshGraphApp = () => {
  const [nodePositions, setNodePositions] = createStore({});

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
    Object.keys(paths).forEach((path, i) => {
      if (!nodePositions[path]) {
        setNodePositions(path, {
          x: 350 + ((i * 180) % 800),
          y: 150 + Math.floor(i / 4) * 180,
        });
      }
    });

    agents.forEach((agent, i) => {
      if (!nodePositions[agent.cid]) {
        setNodePositions(agent.cid, { 
          x: 120, 
          y: 120 + i * 120 
        });
      }
    });
  });

  const handleMove = (id, dx, dy) => {
    setNodePositions(id, (prev) => ({
      x: (prev?.x || 0) + dx,
      y: (prev?.y || 0) + dy,
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
    <div class="w-full h-full bg-slate-950 overflow-hidden relative">
      <MeshMap isWindowed={true} />

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
      </div>
    </div>
  );
};
