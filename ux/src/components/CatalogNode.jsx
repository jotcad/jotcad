import { createSignal, For, Show } from 'solid-js';
import { blackboard } from '../lib/blackboard';
import { Database, RefreshCw, Minus, Maximize2 } from 'lucide-solid';

export const CatalogNode = (props) => {
  const schemas = () => Object.entries(blackboard.schemas() || {});
  const [isRefreshing, setIsRefreshing] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(false);
  const status = () => blackboard.discoveryStatus();

  const refreshCatalog = async () => {
    setIsRefreshing(true);
    await blackboard.discoverSchemas();
    setIsRefreshing(false);
  };

  return (
    <div
      class={`absolute top-4 right-4 z-40 w-72 flex flex-col gap-2 p-4 rounded-xl border border-white/10 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden transition-all ${
        isMinimized() ? 'h-12' : 'max-h-[80vh]'
      }`}
    >
      <div
        class="flex justify-between items-center mb-1 cursor-pointer"
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="flex items-center gap-2 text-white/60">
          <Database size={16} />
          <span class="text-xs font-black uppercase tracking-widest">
            Provider Catalog
          </span>
          <Show when={status() === 'loading'}>
            <div class="w-2 h-2 rounded-full bg-cyan-400 animate-pulse" />
          </Show>
          <Show when={status() === 'error'}>
            <div
              class="w-2 h-2 rounded-full bg-red-500"
              title="Discovery Error"
            />
          </Show>
        </div>
        <div class="flex items-center gap-2">
          <button
            onClick={(e) => {
              e.stopPropagation();
              refreshCatalog();
            }}
            class={`text-white/40 hover:text-white transition-all ${
              isRefreshing() ? 'animate-spin' : ''
            }`}
          >
            <RefreshCw size={14} />
          </button>
          <button
            onClick={(e) => {
              e.stopPropagation();
              setIsMinimized(!isMinimized());
            }}
            class="text-white/20 hover:text-white transition-colors"
          >
            {isMinimized() ? <Maximize2 size={12} /> : <Minus size={12} />}
          </button>
        </div>
      </div>

      <Show when={!isMinimized()}>
        <div class="flex-1 overflow-y-auto custom-scrollbar flex flex-col gap-1">
          <For each={schemas()}>
            {([path, schema]) => (
              <div class="text-[10px] font-mono text-cyan-200/70 py-1.5 px-2 hover:bg-white/5 rounded cursor-pointer transition-colors flex justify-between items-center group">
                <span class="truncate">{path}</span>
                <span class="text-white/20 text-[8px]">
                  {Object.keys(schema).length} params
                </span>
              </div>
            )}
          </For>
          <Show when={status() === 'empty'}>
            <div class="text-[10px] text-white/20 italic p-2 text-center">
              No providers discovered.
            </div>
          </Show>
          <Show when={status() === 'error'}>
            <div class="text-[10px] text-red-400 italic p-2 text-center">
              Discovery failed!
            </div>
          </Show>
        </div>
      </Show>
    </div>
  );
};
