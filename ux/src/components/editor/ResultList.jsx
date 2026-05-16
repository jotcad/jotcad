import { Index, For, Show, createSignal, onMount } from 'solid-js';
import { Layers, Database, Activity } from 'lucide-solid';
import { Viewport } from '../viewport/Viewport';
import { vfs } from '../../lib/blackboard';

export const ResultList = (props) => {
  const downloadFile = async (file) => {
    try {
      const stream = await vfs.read(file.selector);
      if (!stream) throw new Error('Failed to read file from VFS');

      // Drain stream into a Blob
      const reader = stream.getReader();
      const chunks = [];
      while (true) {
        const { done, value } = await reader.read();
        if (done) break;
        chunks.push(value);
      }

      const mimeType = file.label.endsWith('.stl') ? 'application/sla' : 
                       file.label.endsWith('.pdf') ? 'application/pdf' : 
                       'application/octet-stream';
                       
      const blob = new Blob(chunks, { type: mimeType });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = file.label;
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    } catch (err) {
      console.error('[ResultList] Download failed:', err);
    }
  };

  return (
    <div class="flex-1 flex flex-col gap-2 min-h-0">
      <Show when={props.files.length > 0}>
        <div class="flex flex-wrap gap-2 py-1 shrink-0 px-1">
          <For each={props.files}>
            {(file) => (
              <button
                onClick={() => downloadFile(file)}
                class="px-2 py-1 bg-white/10 hover:bg-white/20 border border-white/10 rounded text-[10px] font-mono text-cyan-400 transition-colors flex items-center gap-1"
              >
                <Database size={10} />
                {file.label}
              </button>
            )}
          </For>
        </div>
      </Show>

      <div class="flex-1 overflow-y-auto custom-scrollbar pr-1 flex flex-col gap-4 h-full">
          <For 
            each={props.outputs} 
            fallback={
              <div class="flex-1 min-h-[200px] flex flex-col items-center justify-center gap-2 opacity-20 border-2 border-white/10 rounded-xl shrink-0">
                <Layers size={32} />
                <span class="text-[10px] font-black uppercase tracking-widest">No Outputs Defined</span>
              </div>
            }
          >
            {(out) => {
              const res = () => props.results.find(r => r.port === out.name);
              return (
                <div class="flex-1 min-h-[300px] flex flex-col gap-2 shrink-0">
                  <div 
                      class="w-full flex-1 bg-black/20 rounded-xl border-2 border-white/10 overflow-hidden relative group shrink-0"
                  >
                      <Show when={res()} fallback={
                        <div class="w-full h-full flex flex-col items-center justify-center gap-2 opacity-20 select-none">
                           <Activity size={32} />
                           <span class="text-[9px] font-black uppercase tracking-widest">No Result for '{out.name}'</span>
                        </div>
                      }>
                        <Viewport data={res().data} edgeThreshold={props.edgeThreshold} />
                      </Show>
                      
                      <div class="absolute top-2 left-2 px-1.5 py-0.5 bg-black/60 rounded text-[9px] font-black tracking-widest text-cyan-400 border border-cyan-400/20 pointer-events-none group-hover:bg-cyan-400 group-hover:text-black transition-all">
                        {out.name} ({out.type.split(':').pop()})
                      </div>

                      <Show when={res()}>
                          <div class="absolute top-2 right-2 flex flex-col items-end gap-1 opacity-0 group-hover:opacity-100 transition-opacity">
                              <div class="flex items-center gap-2 bg-black/60 rounded px-2 py-1 border border-white/10">
                                  <span class="text-[9px] font-black uppercase text-white/60">Edges</span>
                                  <input 
                                      type="range" min="0" max="90" step="1" 
                                      value={props.edgeThreshold} 
                                      onInput={e => props.setEdgeThreshold(parseInt(e.target.value))}
                                      class="w-16 h-1 bg-white/10 rounded-lg appearance-none cursor-pointer accent-cyan-400"
                                  />
                              </div>
                          </div>
                      </Show>

                      <div class="absolute bottom-2 right-2 px-1.5 py-0.5 bg-black/60 rounded text-[8px] font-mono text-white/40 pointer-events-none group-hover:text-white/80 transition-colors">
                      {res() ? 'LOCAL VIEWPORT' : 'WAITING FOR DATA'}
                      </div>
                  </div>
                </div>
              );
            }}
          </For>
      </div>
    </div>
  );
};
