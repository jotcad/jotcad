import { createSignal, For, Show, onMount } from 'solid-js';
import interact from 'interactjs';
import { blackboard } from '../../lib/blackboard';
import { Database, RefreshCw, Minus, Maximize2, Plus, Edit2 } from 'lucide-solid';

export const CatalogNode = (props) => {
  const schemas = () => Object.entries(blackboard.schemas() || {});
  const [isRefreshing, setIsRefreshing] = createSignal(false);
  const [newName, setNewName] = createSignal('');
  const status = () => blackboard.discoveryStatus();

  const refreshCatalog = async () => {
    setIsRefreshing(true);
    await blackboard.discoverSchemas();
    setIsRefreshing(false);
  };

  const handleCreate = (e) => {
    e.preventDefault();
    if (!newName().trim()) return;
    blackboard.createNewOp(newName().trim());
    setNewName('');
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

        <form onSubmit={handleCreate} class="flex gap-2 mb-6 shrink-0">
            <input 
              class="flex-1 bg-black/40 border border-white/20 rounded-xl px-4 py-3 text-readable text-white focus:outline-none focus:border-cyan-400/60 shadow-inner"
              placeholder="New op name..."
              value={newName()}
              onInput={e => setNewName(e.target.value)}
            />
            <button 
              type="submit"
              class="tap-target px-4 rounded-xl bg-cyan-400 text-black hover:bg-cyan-300 transition-all border border-cyan-400 shadow-lg"
            >
                <Plus size={24} stroke-width={3} />
            </button>
            <button
                type="button"
                onClick={refreshCatalog}
                class={`tap-target px-4 rounded-xl bg-white/10 text-white hover:bg-white/20 hover:text-cyan-400 transition-all border border-white/10 shadow-lg ${isRefreshing() ? 'animate-spin' : ''}`}
            >
                <RefreshCw size={20} />
            </button>
        </form>

        <div class="flex-1 overflow-y-auto custom-scrollbar flex flex-col gap-2">
          <For each={schemas()}>
            {([path, schema]) => (
                <div 
                  onClick={() => blackboard.openOp(path)}
                  class="text-readable font-mono text-cyan-200/80 py-3 px-3 hover:bg-white/10 rounded-lg cursor-pointer transition-colors flex justify-between items-center group border border-transparent hover:border-white/10"
                >
                  <span class="truncate">{path}</span>
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
