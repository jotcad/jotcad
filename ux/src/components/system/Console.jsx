import { createSignal, For, Show } from 'solid-js';
import { blackboard } from '../../lib/blackboard';
import { Terminal, X, ChevronDown, ChevronUp, Trash2 } from 'lucide-solid';

export const Console = () => {
  const [isOpen, setIsOpen] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(true);
  const logs = () => blackboard.logs();

  return (
    <div class="fixed bottom-20 left-4 z-[60] flex flex-col pointer-events-none">
      <button
        onClick={() => {
          setIsOpen(!isOpen());
          if (isOpen()) setIsMinimized(false);
        }}
        class="pointer-events-auto flex items-center gap-2 px-3 py-1.5 rounded-full bg-black/80 backdrop-blur-md border border-white/20 shadow-xl text-white/60 hover:text-white transition-all"
      >
        <Terminal size={14} />
        <span class="text-[10px] font-black uppercase tracking-widest">Console</span>
        <Show when={logs().length > 0}>
          <span class="bg-cyan-500 text-black text-[9px] px-1.5 rounded-full font-bold">
            {logs().length}
          </span>
        </Show>
      </button>

      <Show when={isOpen()}>
        <div 
          class={`pointer-events-auto mt-2 w-[90vw] md:w-[600px] flex flex-col rounded-xl border border-white/10 bg-black/90 backdrop-blur-3xl shadow-2xl overflow-hidden transition-all duration-300 ${
            isMinimized() ? 'h-12' : 'h-64 md:h-96'
          }`}
        >
          <div class="flex justify-between items-center p-2 border-b border-white/5 bg-white/5">
            <div class="flex items-center gap-2 text-white/40 px-2">
              <Terminal size={12} />
              <span class="text-[9px] font-bold uppercase tracking-widest">Debug Output</span>
            </div>
            <div class="flex items-center gap-1">
              <button 
                onClick={() => setIsMinimized(!isMinimized())}
                class="p-1.5 text-white/20 hover:text-white transition-colors"
              >
                {isMinimized() ? <ChevronUp size={14} /> : <ChevronDown size={14} />}
              </button>
              <button 
                onClick={() => setIsOpen(false)}
                class="p-1.5 text-white/20 hover:text-red-400 transition-colors"
              >
                <X size={14} />
              </button>
            </div>
          </div>

          <Show when={!isMinimized()}>
            <div class="flex-1 overflow-y-auto p-2 font-mono text-[10px] leading-relaxed custom-scrollbar bg-black/40">
              <For each={logs()}>
                {(log) => (
                  <div class={`py-1 border-b border-white/5 whitespace-pre-wrap ${
                    log.type === 'error' ? 'text-red-400' : 
                    log.type === 'warn' ? 'text-amber-400' : 'text-cyan-100/70'
                  }`}>
                    <span class="opacity-30 mr-2">[{new Date(log.t).toLocaleTimeString()}]</span>
                    {log.msg}
                  </div>
                )}
              </For>
            </div>
          </Show>
        </div>
      </Show>
    </div>
  );
};
