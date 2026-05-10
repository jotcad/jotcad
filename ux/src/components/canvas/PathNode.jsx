import {
  createSignal,
  onMount,
  Show,
  createEffect,
  createMemo,
  untrack,
  For
} from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard } from '../../lib/blackboard';
import {
  captureThumbnail
} from '../../lib/render/Thumbnailer';
import { Viewport } from '../viewport/Viewport';
import { StatusBadge } from './StatusBadge';
import {
  Box,
  Triangle,
  Hexagon as HexagonIcon,
  FileJson,
  Layers,
  Clock,
  Activity
} from 'lucide-solid';

/**
 * A draggable Path Node (Circular Resource Container).
 */
export const PathNode = (props) => {
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
          if (blackboard.gestureOwner() === 'blackboard') return;
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
            const gd = await vfs.readData(data.geometry);
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
            (rawData.geometry || rawData.components)
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

  return (
    <div
      ref={nodeRef}
      data-id={props.path}
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

        <div class="px-2 py-0.5 rounded-full text-[8px] font-black tracking-widest whitespace-nowrap bg-blackboard/80 border border-white/5 text-white/60 shadow-lg uppercase">
          {(props.path || 'Unknown').split('/').pop()}
        </div>
      </Show>

      <Show when={isExpanded()}>
        <div class="w-[90vw] md:w-[500px] h-[80vh] rounded-2xl p-3 md:p-4 flex flex-col text-white bg-blackboard border-2 border-cyan-400 shadow-2xl overflow-hidden">
          <div class="flex items-center justify-between border-b border-white/10 pb-2 shrink-0">
            <div class="flex items-center gap-2 relative">
              <StatusBadge
                states={currentStates()}
                hasThumbnail={!!thumbnail()}
                isGenerating={isGenerating()}
                resultCount={props.results.length}
              />
              <Layers size={16} class="opacity-50 text-available" />
              <span class="text-[10px] md:text-xs font-black tracking-widest truncate max-w-[150px] md:max-w-none">
                {props.path}
              </span>
            </div>
            <button
              class="text-[10px] font-bold opacity-40 hover:opacity-100 hover:text-cyan-400 transition-colors"
              onClick={() => setIsExpanded(false)}
            >
              CLOSE
            </button>
          </div>

          <div class="flex-1 min-h-0 flex flex-col gap-2">
            <div class="w-full flex-1 bg-black/40 rounded-xl border-2 border-white/5 overflow-hidden relative group mt-2">
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

            <div class="flex flex-col gap-2 mt-3 max-h-[30vh] overflow-y-auto pr-1 custom-scrollbar shrink-0">
                <For each={props.results}>
                    {(res) => (
                        <div class="bg-black/40 rounded-lg p-2 border border-white/5 flex flex-col gap-1.5 shrink-0">
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
            </div>
          </div>

          <div class="mt-auto shrink-0 pt-2 flex flex-col gap-1 border-t border-white/5">
            <Show when={shapeData()}>
              <div class="flex flex-col gap-1">
                <button
                  onClick={() => setShowShape(!showShape())}
                  class="flex items-center justify-between w-full px-2 py-1 bg-white/5 hover:bg-white/10 hover:text-cyan-400 rounded transition-colors text-left"
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
                  <pre class="text-[8px] font-mono p-2 bg-black/60 rounded border border-white/5 text-cyan-400/80 overflow-x-auto whitespace-pre-wrap max-h-32 overflow-y-auto custom-scrollbar">
                    {JSON.stringify(shapeData(), null, 2)}
                  </pre>
                </Show>
              </div>
            </Show>

            <Show when={geoData()}>
              <div class="flex flex-col gap-1">
                <button
                  onClick={() => setShowGeo(!showGeo())}
                  class="flex items-center justify-between w-full px-2 py-1 bg-white/5 hover:bg-white/10 hover:text-cyan-400 rounded transition-colors text-left"
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
                  <pre class="text-[8px] font-mono p-2 bg-black/60 rounded border border-white/5 text-green-400/80 max-h-32 overflow-auto custom-scrollbar whitespace-pre">
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
