import { createMemo, For, createEffect, onMount, untrack, createSignal } from 'solid-js';
import { createStore } from 'solid-js/store';
import { blackboard } from '../../lib/blackboard';
import { MeshMap } from './MeshMap';
import { PathNode } from '../canvas/PathNode';
import { AgentNode } from '../canvas/AgentNode';
import { Connection } from '../canvas/Connection';

export const MeshGraphApp = () => {
  const [nodePositions, setNodePositions] = createStore({});
  const [view, setView] = createSignal({ x: 0, y: -150, scale: 0.8 });
  
  let isPanning = false;
  let startPanMouse = { x: 0, y: 0 };
  let startPanView = { x: 0, y: 0 };

  const handlePointerDown = (e) => {
    if (e.button !== 0) return; // Left click only
    const target = e.target;
    // Don't initiate background panning if clicking on a draggable node/handle (excluding the root itself)
    const grabNode = target.closest('.cursor-grab');
    if (target.closest('button, input, select, textarea') || (grabNode && grabNode !== e.currentTarget)) return;

    isPanning = true;
    startPanMouse = { x: e.clientX, y: e.clientY };
    startPanView = { ...view() };
    e.currentTarget.setPointerCapture(e.pointerId);
  };

  const handlePointerMove = (e) => {
    if (!isPanning) return;
    const dx = e.clientX - startPanMouse.x;
    const dy = e.clientY - startPanMouse.y;
    setView({
      x: startPanView.x + dx,
      y: startPanView.y + dy,
      scale: startPanView.scale
    });
  };

  const handlePointerUp = (e) => {
    if (!isPanning) return;
    isPanning = false;
    e.currentTarget.releasePointerCapture(e.pointerId);
  };

  const handleWheel = (e) => {
    e.preventDefault();
    const rect = e.currentTarget.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const delta = e.deltaY;
    const scaleFactor = delta > 0 ? 0.9 : 1.1;

    setView((prev) => {
      const newScale = Math.min(Math.max(prev.scale * scaleFactor, 0.15), 4);
      const worldX = (mouseX - prev.x) / prev.scale;
      const worldY = (mouseY - prev.y) / prev.scale;

      const newX = mouseX - worldX * newScale;
      const newY = mouseY - worldY * newScale;

      return { x: newX, y: newY, scale: newScale };
    });
  };

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
      x: (prev?.x || 0) + dx / view().scale,
      y: (prev?.y || 0) + dy / view().scale,
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
              opacity: 0.8,
              strokeWidth: '3.5'
            });
          }
        }
      });
    });

    return conns;
  });

  return (
    <div 
      class="w-full h-full bg-slate-950 overflow-hidden relative cursor-grab active:cursor-grabbing select-none"
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerUp}
      onWheel={handleWheel}
    >
      <div 
        class="absolute inset-0 pointer-events-none origin-top-left"
        style={{
          transform: `translate(${view().x}px, ${view().y}px) scale(${view().scale})`,
        }}
      >
        <div class="absolute inset-0 pointer-events-none">
          <MeshMap isWindowed={true} scale={view().scale} />
        </div>

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
                  strokeWidth={conn.strokeWidth}
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
    </div>
  );
};
