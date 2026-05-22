import { createSignal, For, Show, onMount } from 'solid-js';
import interact from 'interactjs';
import { blackboard } from '../../lib/blackboard';
import { Database, RefreshCw, Minus, Maximize2, Plus, Edit2 } from 'lucide-solid';

export const CatalogNode = (props) => {
  const [filter, setFilter] = createSignal('');
  
  const schemas = () => {
    const raw = Object.entries(blackboard.schemas() || {});
    const groups = new Map();

    for (const [path, schema] of raw) {
        // user/Foot5:v2 -> base: user/Foot5, v: 2
        const [base, vStr] = path.split(':v');
        const version = vStr ? parseInt(vStr) : 0;

        if (!groups.has(base) || groups.get(base).version < version) {
            groups.set(base, { path, schema, version });
        }
    }

    const search = filter().toLowerCase();
    // Return the latest versioned path for each base name, filtered and sorted
    return Array.from(groups.values())
      .filter(g => g.path.toLowerCase().includes(search))
      .sort((a, b) => a.path.localeCompare(b.path))
      .map(g => [g.path, g.schema]);
  };
  const [isRefreshing, setIsRefreshing] = createSignal(false);
  const status = () => blackboard.discoveryStatus();

  const refreshCatalog = async () => {
    setIsRefreshing(true);
    await blackboard.discoverSchemas();
    setIsRefreshing(false);
  };

  const [selectedPath, setSelectedPath] = createSignal(null);
  const selectedSchema = () => {
    const path = selectedPath();
    if (!path) return null;
    return blackboard.schemas()[path];
  };

  const content = () => (
    <div class="catalog-content flex flex-col h-full gap-2 p-3 md:p-6 bg-transparent overflow-hidden">
        <Show when={!props.isWindowed}>
            <div class="flex justify-between items-center mb-1 text-white/60">
              <div class="flex items-center gap-2">
                <Database size={16} />
                <span class="text-ui-label font-black uppercase tracking-widest">Provider Catalog</span>
              </div>
            </div>
        </Show>

        <div class="flex gap-2 mb-6 shrink-0">
            <input 
              class="flex-1 bg-black/40 border border-white/20 rounded-xl px-4 py-3 text-readable text-white focus:outline-none focus:border-cyan-400/60 shadow-inner"
              placeholder="Filter operators..."
              value={filter()}
              onInput={e => setFilter(e.target.value)}
            />
            <button
                type="button"
                onClick={refreshCatalog}
                class={`tap-target px-4 rounded-xl bg-white/10 text-white hover:bg-white/20 hover:text-cyan-400 transition-all border border-white/10 shadow-lg ${isRefreshing() ? 'animate-spin' : ''}`}
            >
                <RefreshCw size={20} />
            </button>
        </div>

        <div class="flex-1 overflow-y-auto custom-scrollbar flex flex-col gap-2">
          <For each={schemas()}>
            {([path, schema]) => (
                <div 
                  onClick={() => setSelectedPath(path === selectedPath() ? null : path)}
                  class={`text-readable font-mono py-3 px-3 hover:bg-white/10 rounded-lg cursor-pointer transition-all flex flex-col gap-2 border ${selectedPath() === path ? 'border-cyan-400 bg-white/5 shadow-lg' : 'border-transparent text-cyan-200/80 hover:border-white/10'}`}
                >
                  <div class="flex justify-between items-center w-full">
                    <span class="truncate font-bold">{path}</span>
                    <Show when={path.startsWith('user/')}>
                      <button 
                        onClick={(e) => { e.stopPropagation(); blackboard.openOp(path); }}
                        class="p-2 text-white/40 hover:text-cyan-400 transition-colors"
                      >
                        <Edit2 size={16} />
                      </button>
                    </Show>
                  </div>

                  <Show when={selectedPath() === path}>
                    <div class="text-xs text-white/60 font-sans border-t border-white/10 pt-2 flex flex-col gap-3">
                      <p class="italic">{schema.description || 'No description provided.'}</p>
                      
                      <Show when={schema.arguments && schema.arguments.length > 0}>
                        <div>
                          <p class="font-bold text-cyan-400/80 mb-1 uppercase tracking-tighter">Arguments</p>
                          <ul class="flex flex-col gap-1">
                            <For each={schema.arguments}>
                              {(arg) => (
                                <li class="flex justify-between font-mono bg-black/20 p-1 rounded">
                                  <span>{arg.name}</span>
                                  <span class="text-white/40">{arg.type}</span>
                                </li>
                              )}
                            </For>
                          </ul>
                        </div>
                      </Show>

                      <Show when={schema.outputs}>
                        <div>
                          <p class="font-bold text-pink-400/80 mb-1 uppercase tracking-tighter">Outputs</p>
                          <ul class="flex flex-col gap-1">
                            <For each={Object.entries(schema.outputs)}>
                              {([port, out]) => (
                                <li class="flex justify-between font-mono bg-black/20 p-1 rounded">
                                  <span>{port}</span>
                                  <span class="text-white/40">{out.type}</span>
                                </li>
                              )}
                            </For>
                          </ul>
                        </div>
                      </Show>
                    </div>
                  </Show>
                </div>
              )
            }
          </For>
        </div>
    </div>
  );

  return (
    <Show when={props.isWindowed} fallback={
        <div class="absolute z-40 w-72 flex flex-col bg-black/80 backdrop-blur-2xl border-2 border-cyan-400 rounded-xl shadow-2xl overflow-hidden" style={{left: '20px', top: '20px'}}>
            {content()}
        </div>
    }>
        {content()}
    </Show>
  );
};
