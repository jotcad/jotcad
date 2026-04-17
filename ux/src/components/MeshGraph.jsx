import { For, Show, createSignal, onMount } from 'solid-js';
import { blackboard } from '../lib/blackboard';
import interact from 'interactjs';
import { Minus, Maximize2 } from 'lucide-solid';

export function MeshGraph() {
  let nodeRef;
  const pulses = blackboard.pulse;
  const [pos, setPos] = createSignal({
    x: window.innerWidth - 340,
    y: window.innerHeight - 450,
  });
  const [size, setSize] = createSignal({ width: 320, height: 400 });
  const [isMinimized, setIsMinimized] = createSignal(false);

  onMount(() => {
    interact(nodeRef)
      .draggable({
        listeners: {
          move(event) {
            setPos((p) => ({ x: p.x + event.dx, y: p.y + event.dy }));
          },
        },
      })
      .resizable({
        edges: { top: true, left: true, bottom: false, right: false },
        listeners: {
          move(event) {
            if (isMinimized()) return;
            setSize((s) => ({
              width: event.rect.width,
              height: event.rect.height,
            }));
            setPos((p) => ({ x: event.rect.left, y: event.rect.top }));
          },
        },
      });
  });

  const getStatusColor = (type) => {
    switch (type) {
      case 'READ_START':
        return 'text-blue-400';
      case 'HANDLER_START':
        return 'text-yellow-400';
      case 'WORKING':
        return 'text-orange-400 animate-pulse';
      case 'SUCCESS':
        return 'text-green-400';
      case 'ERROR':
        return 'text-red-400';
      default:
        return 'text-white/50';
    }
  };

  const formatTopic = (topic) => {
    try {
      if (typeof topic === 'object') {
        return `${topic.path} (${
          Object.keys(topic.parameters || {}).length
        } params)`;
      }
      const [path, params] = topic.split('?');
      if (!params) return path;
      return `${path} (...)`;
    } catch (e) {
      return String(topic);
    }
  };

  return (
    <div
      ref={nodeRef}
      class="fixed flex flex-col gap-2 z-50 pointer-events-auto select-none overflow-hidden"
      style={{
        left: `${pos().x}px`,
        top: `${pos().y}px`,
        width: isMinimized() ? '180px' : `${size().width}px`,
        height: isMinimized() ? '40px' : `${size().height}px`,
        transition: 'width 0.2s, height 0.2s',
      }}
    >
      <div
        class={`p-3 rounded-t-xl bg-black/60 backdrop-blur-xl border-x-2 border-t-2 border-white/10 flex justify-between items-center cursor-move ${
          isMinimized() ? 'rounded-b-xl border-b-2' : ''
        }`}
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="flex items-center gap-2">
          <div
            class={`w-1.5 h-1.5 rounded-full ${
              blackboard.isConnected() ? 'bg-green-500' : 'bg-red-500'
            }`}
          />
          <span class="text-[9px] font-black uppercase tracking-widest text-white/40">
            Mesh Pulse
          </span>
        </div>
        <button
          class="text-white/20 hover:text-white transition-colors"
          onClick={() => setIsMinimized(!isMinimized())}
        >
          {isMinimized() ? <Maximize2 size={12} /> : <Minus size={12} />}
        </button>
      </div>

      <Show when={!isMinimized()}>
        <div class="flex-1 overflow-y-auto p-2 bg-black/40 backdrop-blur-md border-x-2 border-b-2 border-white/10 rounded-b-xl flex flex-col gap-1 custom-scrollbar">
          <Show when={pulses().length === 0}>
            <div class="p-4 text-center text-white/20 text-[10px] uppercase italic text-balance">
              Silent Mesh...
            </div>
          </Show>

          <For each={[...pulses()].reverse()}>
            {(pulse) => (
              <div class="group flex flex-col p-2 rounded-lg bg-white/5 border border-white/5 hover:border-white/10 transition-colors shrink-0">
                <div class="flex justify-between items-start gap-2">
                  <span
                    class={`text-[10px] font-bold ${getStatusColor(
                      pulse.payload.type
                    )}`}
                  >
                    {pulse.payload.type}
                  </span>
                  <span class="text-[8px] font-mono text-white/20">
                    {new Date(pulse.t).toLocaleTimeString([], {
                      hour12: false,
                      hour: '2-digit',
                      minute: '2-digit',
                      second: '2-digit',
                    })}
                  </span>
                </div>
                <div
                  class="text-[11px] font-mono text-white/70 truncate"
                  title={JSON.stringify(pulse.selector)}
                >
                  {formatTopic(pulse.selector || pulse.topic)}
                </div>
                <div class="text-[9px] text-white/30 truncate uppercase tracking-tighter">
                  {pulse.payload.peer || 'unknown-peer'}
                </div>
              </div>
            )}
          </For>
        </div>
      </Show>
    </div>
  );
}
