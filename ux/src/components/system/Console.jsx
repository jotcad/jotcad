import { createSignal, For, Show, createEffect } from 'solid-js';
import { blackboard } from '../../lib/blackboard';
import { logActions } from '../../lib/state/AppState';
import { Terminal, X, ChevronDown, ChevronUp, Trash2 } from 'lucide-solid';

export const Console = (props) => {
  const [isOpen, setIsOpen] = createSignal(false);
  const logs = () => blackboard.logs();
  let logScrollRef;

  createEffect(() => {
    // Auto-scroll to bottom on new logs
    if (logScrollRef && logs().length) {
      logScrollRef.scrollTop = logScrollRef.scrollHeight;
    }
  });

  const content = () => (
    <div class="flex flex-col h-full bg-black/40 font-mono text-[10px] leading-relaxed overflow-hidden">
        <div class="flex items-center justify-between px-3 py-1.5 border-b border-white/10 bg-white/5 shrink-0">
             <div class="flex items-center gap-2 text-white/40">
                 <Terminal size={12} />
                 <span class="text-[9px] font-black uppercase tracking-widest">System Logs</span>
             </div>
             <button 
                onClick={() => logActions.clear()}
                class="p-1 text-white/20 hover:text-red-400 transition-colors"
                title="Clear Logs"
             >
                 <Trash2 size={12} />
             </button>
        </div>
        <div ref={logScrollRef} class="flex-1 overflow-y-auto p-2 custom-scrollbar">
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
    </div>
  );

  return (
    <Show when={props.isWindowed} fallback={
        <div class="fixed bottom-20 left-4 z-[60] flex flex-col pointer-events-none">
          <button
            onClick={() => setIsOpen(!isOpen())}
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
            <div class="pointer-events-auto mt-2 w-[90vw] md:w-[600px] h-64 md:h-96 rounded-xl border border-white/10 bg-black/90 backdrop-blur-3xl shadow-2xl overflow-hidden flex flex-col">
                {content()}
            </div>
          </Show>
        </div>
    }>
        {content()}
    </Show>
  );
};
