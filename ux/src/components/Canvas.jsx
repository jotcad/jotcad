import {
  createSignal,
  onMount,
  onCleanup,
  For,
  Show,
  createEffect,
  createMemo,
  untrack,
} from 'solid-js';
import { createStore, reconcile } from 'solid-js/store';
import interact from 'interactjs';
import { blackboard, vfs } from '../lib/blackboard';
import {
  initSharedRenderer,
  createScene,
  updateViewports,
  captureThumbnail,
  renderJotToScene,
} from '../lib/three_utils';
import { JotNode } from './JotNode';
import { MeshMap } from './MeshMap';
import { Viewport } from './Viewport';
import { DynamicUX } from './DynamicUX';
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
  Check,
} from 'lucide-solid';

/**
 * A list of status pips laid out contiguously in a clockwise direction along the border.
 */
const StatusBadge = (props) => {
  // 1. Gather active pips
  const badges = [];

  // Grey: Schema
  if (props.states.includes('SCHEMA')) {
    badges.push({
      id: 'schema',
      color: 'bg-slate-600',
      title: 'Schema Declared',
      icon: <FileJson size={10} class="text-white" />,
    });
  }

  // White/Yellow/Blue: Availability State
  if (props.states.includes('AVAILABLE')) {
    badges.push({
      id: 'available',
      color: 'bg-white',
      borderColor: 'border-node',
      title: 'Available',
      icon: <Check size={10} class="text-node stroke-[4]" />,
    });
  } else if (props.states.includes('PROVISIONING')) {
    badges.push({
      id: 'provisioning',
      color: 'bg-provisioning',
      title: 'Provisioning',
      animate: 'animate-pulse',
    });
  } else if (props.states.includes('PENDING')) {
    badges.push({
      id: 'pending',
      color: 'bg-pending',
      title: 'Pending',
      animate: 'animate-bounce',
    });
  }

  // Blue/Cyan: Thumbnail
  if (props.hasThumbnail) {
    badges.push({
      id: 'thumb-ready',
      color: 'bg-blue-600',
      title: 'Thumbnail Ready',
      icon: <CheckCircle2 size={10} class="text-white" />,
    });
  } else if (props.isGenerating) {
    badges.push({
      id: 'thumb-gen',
      color: 'bg-cyan-500',
      title: 'Generating Thumbnail...',
      animate: 'animate-pulse',
      icon: <Activity size={10} class="text-white" />,
    });
  }

  // White: Result Count
  if (props.resultCount > 1) {
    badges.push({
      id: 'count',
      color: 'bg-white',
      title: `${props.resultCount} results`,
      content: (
        <span class="text-blackboard text-[8px] font-black">
          {props.resultCount}
        </span>
      ),
    });
  }

  // 2. Define clockwise positions (Top -> Right -> Bottom -> Left)
  // Offset from the chip edge (-6px means slightly overlapping the border)
  const positions = [
    { top: '-6px', left: '-6px' }, // Pos 0: Top-Left
    { top: '-6px', left: '14px' }, // Pos 1
    { top: '-6px', left: '34px' }, // Pos 2
    { top: '-6px', left: '54px' }, // Pos 3: Top-Right
    { top: '14px', left: '54px' }, // Pos 4
    { top: '34px', left: '54px' }, // Pos 5
    { top: '54px', left: '54px' }, // Pos 6: Bottom-Right
    { top: '54px', left: '34px' }, // Pos 7
    { top: '54px', left: '14px' }, // Pos 8
    { top: '54px', left: '-6px' }, // Pos 9: Bottom-Left
  ];

  return (
    <For each={badges}>
      {(badge, i) => (
        <div
          class={`absolute w-4 h-4 rounded-full border border-blackboard flex items-center justify-center shadow-lg z-20 transition-all duration-300 ${
            badge.color
          } ${badge.borderColor || ''} ${badge.animate || ''}`}
          style={{
            top: positions[i()]?.top || '0px',
            left: positions[i()]?.left || '0px',
          }}
          title={badge.title}
        >
          {badge.icon || badge.content}
        </div>
      )}
    </For>
  );
};

/**
 * A draggable Path Node (Circular Resource Container).
 */
const PathNode = (props) => {
  let nodeRef;
  const [isExpanded, setIsExpanded] = createSignal(false);
  const [thumbnail, setThumbnail] = createSignal(null);
  const [isGenerating, setIsGenerating] = createSignal(false);
  const [shapeData, setShapeData] = createSignal(null);
  const [geoData, setGeoData] = createSignal(null);
  const [showShape, setShowShape] = createSignal(false);
  const [showGeo, setShowGeo] = createSignal(false);

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          props.onMove(props.path, event.dx, event.dy);
        },
      },
    });
  });

  // Fetch data for dynamic viewing when expanded
  createEffect(async () => {
    if (isExpanded() && !shapeData()) {
      const hasAvailable = props.results.find((r) => r.state === 'AVAILABLE');
      if (hasAvailable) {
        try {
          const data = await vfs.readData(
            hasAvailable.path,
            hasAvailable.parameters
          );
          setShapeData(data);

          // If it's a shape, also fetch its geometry for the expandable section
          if (typeof data === 'object' && data.geometry) {
            const gd = await vfs.readData(
              data.geometry.path,
              data.geometry.parameters
            );
            if (gd) {
              const text =
                typeof gd === 'string' ? gd : new TextDecoder().decode(gd);
              setGeoData(text);
            }
          }
        } catch (e) {
          console.warn('[PathNode] Failed to fetch view data:', e);
        }
      }
    }
  });

  // Auto-generate thumbnail if available and is a mesh/shape
  createEffect(async () => {
    const results = props.results;
    const hasAvailable = results.find((r) => r.state === 'AVAILABLE');
    const schema = results.find((r) => r.state === 'SCHEMA');

    // We only want this effect to depend on 'results'.
    // We 'untrack' our local status signals to avoid recursive loops.
    const isBusy = untrack(isGenerating);
    const hasThumb = untrack(thumbnail);

    const isGeometry =
      props.path.startsWith('jot/') ||
      props.path.includes('mesh') ||
      props.path.includes('triangle') ||
      props.path.includes('box') ||
      (schema && schema.data?.type === 'mesh');

    if (hasAvailable && !hasThumb && !isBusy && isGeometry) {
      setIsGenerating(true);
      try {
        const rawData = await vfs.readData(
          hasAvailable.path,
          hasAvailable.parameters
        );
        if (rawData) {
          let img = null;
          if (
            typeof rawData === 'string' &&
            (rawData.startsWith('\n=') || rawData.includes('v '))
          ) {
            img = await captureThumbnail(vfs, rawData);
          } else if (
            typeof rawData === 'object' &&
            (rawData.geometry || rawData.shapes)
          ) {
            const jot = `\n=${
              JSON.stringify(rawData).length
            } files/main.json\n${JSON.stringify(rawData)}`;
            img = await captureThumbnail(vfs, jot);
          }
          if (img) setThumbnail(img);
        }
      } catch (e) {
        console.warn('[Thumbnail] Failed to generate:', e);
      } finally {
        setIsGenerating(false);
      }
    }
  });

  const TypeIcon = () => {
    const p = props.path.toLowerCase();
    const size = isExpanded() ? 20 : 24;
    if (p.includes('box')) return <Box size={size} class="opacity-20" />;
    if (p.includes('triangle'))
      return <Triangle size={size} class="opacity-20" />;
    if (p.includes('mesh'))
      return <HexagonIcon size={size} class="opacity-20" />;
    return <FileJson size={size} class="opacity-20" />;
  };

  const currentStates = () => props.results.map((r) => r.state);
  const schemaResult = createMemo(() =>
    props.results.find((r) => r.state === 'SCHEMA')
  );

  return (
    <div
      ref={nodeRef}
      class={`absolute select-none cursor-move flex flex-col items-center gap-2 ${
        isExpanded() ? 'z-50' : 'z-10'
      }`}
      style={{
        left: `${props.pos?.x || 0}px`,
        top: `${props.pos?.y || 0}px`,
        'pointer-events': 'auto',
      }}
      onDblClick={() => setIsExpanded(!isExpanded())}
    >
      <Show when={!isExpanded()}>
        {/* The Chip Body */}
        <div class="w-16 h-16 rounded-xl border-2 border-white/10 shadow-2xl flex items-center justify-center bg-black/40 backdrop-blur-md relative group hover:border-white/30 transition-all overflow-visible">
          <StatusBadge
            states={currentStates()}
            hasThumbnail={!!thumbnail()}
            isGenerating={isGenerating()}
            resultCount={props.results.length}
          />

          <Show when={thumbnail()} fallback={<TypeIcon />}>
            <img
              src={thumbnail()}
              class="w-full h-full object-cover rounded-lg p-0.5"
            />
          </Show>
        </div>

        {/* Path Label Below */}
        <div class="px-2 py-0.5 rounded-full text-[8px] font-black tracking-widest whitespace-nowrap bg-blackboard/80 border border-white/5 text-white/60 shadow-lg uppercase">
          {props.path.split('/').pop()}
        </div>
      </Show>

      <Show when={isExpanded()}>
        <div class="w-96 h-auto rounded-xl p-4 flex flex-col text-white bg-blackboard border-2 border-white/10 shadow-2xl">
          <div class="flex items-center justify-between border-b border-white/10 pb-2">
            <div class="flex items-center gap-2 relative">
              <StatusBadge
                states={currentStates()}
                hasThumbnail={!!thumbnail()}
                isGenerating={isGenerating()}
                resultCount={props.results.length}
              />
              <Layers size={16} class="opacity-50 text-available" />
              <span class="text-xs font-black tracking-widest truncate">
                {props.path}
              </span>
            </div>
            <button
              class="text-[10px] font-bold opacity-40 hover:opacity-100 hover:text-red-400 transition-colors"
              onClick={() => setIsExpanded(false)}
            >
              CLOSE
            </button>
          </div>

          <div class="w-full h-64 bg-black/40 rounded-lg border border-white/5 overflow-hidden relative group mt-2">
            <Show
              when={
                shapeData() &&
                (props.path.startsWith('jot/') ||
                  props.path.includes('mesh') ||
                  props.path.includes('triangle') ||
                  props.path.includes('box'))
              }
              fallback={
                <Show when={thumbnail()}>
                  <img
                    src={thumbnail()}
                    class="w-full h-full object-contain opacity-50"
                  />
                </Show>
              }
            >
              <Viewport data={shapeData()} />
            </Show>
            <div class="absolute bottom-2 right-2 px-1.5 py-0.5 bg-black/60 rounded text-[8px] font-mono text-white/40 pointer-events-none group-hover:text-white/80 transition-colors">
              INTERACTIVE VIEW
            </div>
          </div>

          <div class="flex flex-col gap-2 mt-3 max-h-[50vh] overflow-y-auto pr-1 custom-scrollbar">
            <For each={props.results}>
              {(res) => (
                <div class="bg-black/40 rounded-lg p-2 border border-white/5 flex flex-col gap-1.5">
                  <div class="flex justify-between items-center">
                    <span class="text-[8px] font-black tracking-widest text-available">
                      {res.state}
                    </span>
                    <span class="text-[7px] opacity-20 font-mono">
                      {res.cid.slice(0, 8)}
                    </span>
                  </div>
                  <div class="text-[9px] font-mono text-white/70 break-all bg-black/20 p-1.5 rounded">
                    {JSON.stringify(res.parameters)}
                  </div>
                </div>
              )}
            </For>

            {/* Expandable Shape JSON Section */}
            <Show when={shapeData()}>
              <div class="flex flex-col gap-1 border-t border-white/5 pt-2">
                <button
                  onClick={() => setShowShape(!showShape())}
                  class="flex items-center justify-between w-full px-2 py-1 bg-white/5 hover:bg-white/10 rounded transition-colors text-left"
                >
                  <span class="text-[9px] font-black tracking-[0.2em] text-white/40 uppercase">
                    Shape JSON
                  </span>
                  <Activity
                    size={10}
                    class={`transition-transform ${
                      showShape() ? 'rotate-180' : ''
                    }`}
                  />
                </button>
                <Show when={showShape()}>
                  <pre class="text-[8px] font-mono p-2 bg-black/60 rounded border border-white/5 text-cyan-400/80 overflow-x-auto whitespace-pre-wrap">
                    {JSON.stringify(shapeData(), null, 2)}
                  </pre>
                </Show>
              </div>
            </Show>

            {/* Expandable Raw Geometry Section */}
            <Show when={geoData()}>
              <div class="flex flex-col gap-1 border-t border-white/5 pt-2">
                <button
                  onClick={() => setShowGeo(!showGeo())}
                  class="flex items-center justify-between w-full px-2 py-1 bg-white/5 hover:bg-white/10 rounded transition-colors text-left"
                >
                  <span class="text-[9px] font-black tracking-[0.2em] text-white/40 uppercase">
                    Raw Geometry
                  </span>
                  <Layers
                    size={10}
                    class={`transition-transform ${
                      showGeo() ? 'rotate-180' : ''
                    }`}
                  />
                </button>
                <Show when={showGeo()}>
                  <pre class="text-[8px] font-mono p-2 bg-black/60 rounded border border-white/5 text-green-400/80 max-h-48 overflow-auto custom-scrollbar whitespace-pre">
                    {geoData()}
                  </pre>
                </Show>
              </div>
            </Show>

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
        left: `${props.pos?.x || 0}px`,
        top: `${props.pos?.y || 0}px`,
        'pointer-events': 'auto',
      }}
    >
      <svg
        viewBox="0 0 100 100"
        class="absolute inset-0 w-full h-full drop-shadow-2xl"
      >
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
          <div class="text-[5px] opacity-40 font-mono tracking-tighter text-white uppercase">
            {props.data.source}
          </div>
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
      x1={props.fromPos?.x + props.fromSize / 2}
      y1={props.fromPos?.y + props.fromSize / 2}
      x2={props.toPos?.x + props.toSize / 2}
      y2={props.toPos?.y + props.toSize / 2}
      stroke={props.color}
      stroke-width="2"
      stroke-opacity={props.opacity}
      stroke-dasharray={props.dashed ? '4 4' : ''}
    />
  );
};

export const Canvas = () => {
  const [nodePositions, setNodePositions] = createStore({});
  const [view, setView] = createSignal({ x: 0, y: 0, scale: 1 });
  let canvasRef;
  let dragRef;

  onMount(() => {
    blackboard.start();
    const renderer = initSharedRenderer();
    document.body.appendChild(renderer.domElement);

    const animate = () => {
      updateViewports();
      requestAnimationFrame(animate);
    };
    animate();

    interact(dragRef).draggable({
      listeners: {
        move(event) {
          if (event.target === dragRef) {
            setView((v) => ({ ...v, x: v.x + event.dx, y: v.y + event.dy }));
          }
        },
      },
    });
  });

  const handleWheel = (e) => {
    const delta = e.deltaY;
    const scaleFactor = delta > 0 ? 0.9 : 1.1;
    const newScale = Math.min(Math.max(view().scale * scaleFactor, 0.1), 5);

    // Zoom towards mouse position
    const rect = canvasRef.getBoundingClientRect();
    const mouseX = e.clientX - rect.left;
    const mouseY = e.clientY - rect.top;

    const worldX = (mouseX - view().x) / view().scale;
    const worldY = (mouseY - view().y) / view().scale;

    const newX = mouseX - worldX * newScale;
    const newY = mouseY - worldY * newScale;

    setView({ x: newX, y: newY, scale: newScale });
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
        if (!paths[node.path.replace(/@schema$/, '')])
          paths[node.path.replace(/@schema$/, '')] = [];
        paths[node.path.replace(/@schema$/, '')].push(node);
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
          x: 350 + ((i * 180) % (window.innerWidth - 450)),
          y: 150 + Math.floor(i / 4) * 180,
        });
      }
    });

    agents.forEach((agent, i) => {
      if (!nodePositions[agent.cid]) {
        setNodePositions(agent.cid, { x: 120, y: 120 + i * 120 });
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
      onWheel={handleWheel}
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
        </div>
      </div>

      <div class="absolute inset-0 z-50 pointer-events-none">
        <div class="pointer-events-auto">
          <JotNode />
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
          {blackboard.isConnected() ? 'Mesh Online' : 'Mesh Offline'}
        </span>
      </div>

      <div class="absolute bottom-6 left-6 flex flex-col gap-1 pointer-events-none z-50">
        <div class="text-[10px] font-black text-white/20 tracking-[0.4em]">
          JotCAD Distributed Blackboard
        </div>
        <div class="text-[8px] font-mono text-white/10 tracking-widest opacity-50 uppercase text-balance">
          Double-click Path [Circle] to view results • Drag background to Pan •
          Wheel to Zoom
        </div>

        {/* Debug Info */}
        <div class="mt-4 flex flex-col gap-2">
          <div class="text-[8px] font-mono text-white/20">
            PEERS: {blackboard.meshTopology().peers?.length || 0}
          </div>
          <div class="text-[8px] font-mono text-white/20">
            SCHEMAS: {Object.keys(blackboard.schemas()).length}
          </div>
          <div class="max-w-xs flex flex-wrap gap-1">
            <For each={Object.entries(blackboard.schemas())}>
              {([path, s]) => (
                <div class="text-[6px] px-1 bg-white/5 text-white/30 rounded">
                  {path.split('/').pop()} ({s._origin?.slice(0, 6)})
                </div>
              )}
            </For>
          </div>
        </div>
      </div>
    </div>
  );
};
