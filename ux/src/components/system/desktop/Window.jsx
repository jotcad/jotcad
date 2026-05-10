import { createSignal, onMount, Show, createEffect } from 'solid-js';
import { Portal } from 'solid-js/web';
import interact from 'interactjs';
import { X, Maximize2, Minimize2, Minus } from 'lucide-solid';
import { windowActions } from '../../../lib/state/AppState';
import { blackboard } from '../../../lib/blackboard';

export const Window = (props) => {
  let winRef;
  const [isInteracting, setIsInteracting] = createSignal(false);
  
  // Reactive derived signal from store
  const isMaximized = () => props.data.isMaximized || false;

  onMount(() => {
    interact(winRef)
      .draggable({
        allowFrom: '.win-header',
        listeners: {
          start() { setIsInteracting(true); },
          move(event) {
            if (isMaximized()) return;
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            windowActions.update(props.data.id, {
              pos: { 
                x: props.data.pos.x + event.dx / (props.ignoreZoom ? 1 : s), 
                y: props.data.pos.y + event.dy / (props.ignoreZoom ? 1 : s) 
              }
            });
          },
          end() { setIsInteracting(false); }
        }
      })
      .resizable({
        edges: { left: true, right: true, bottom: true, top: false },
        listeners: {
          start() { setIsInteracting(true); },
          move(event) {
            if (isMaximized()) return;
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            windowActions.update(props.data.id, {
              size: {
                width: event.rect.width / (props.ignoreZoom ? 1 : s),
                height: event.rect.height / (props.ignoreZoom ? 1 : s)
              },
              pos: {
                x: props.data.pos.x + event.deltaRect.left / (props.ignoreZoom ? 1 : s),
                y: props.data.pos.y + event.deltaRect.top / (props.ignoreZoom ? 1 : s)
              }
            });
          },
          end() { setIsInteracting(false); }
        },
        modifiers: [
          interact.modifiers.restrictSize({
            min: { width: 300, height: 200 }
          })
        ]
      });
  });

  const toggleMaximize = (e) => {
    e.stopPropagation();
    windowActions.update(props.data.id, { isMaximized: !isMaximized() });
  };

  const windowContent = () => (
    <div
      ref={winRef}
      onPointerDown={(e) => { e.stopPropagation(); windowActions.raise(props.data.id); }}
      class={`jot-window absolute pointer-events-auto rounded-xl border border-white/20 bg-black/90 backdrop-blur-3xl shadow-2xl flex flex-col overflow-hidden ${
        isMaximized() ? 'is-maximized fixed inset-0 z-[1000] rounded-none border-none transition-all duration-300' : 
        isInteracting() ? '' : 'transition-all duration-200'
      }`}
      style={{
        left: isMaximized() ? '0' : `${props.data.pos.x}px`,
        top: isMaximized() ? '0' : `${props.data.pos.y}px`,
        width: isMaximized() ? '100vw' : `${props.data.size.width}px`,
        height: isMaximized() ? '100vh' : `${props.data.size.height}px`,
        'z-index': isMaximized() ? 1000 : props.data.zIndex
      }}
    >
      {/* Header */}
      <div 
        class={`win-header shrink-0 flex items-center justify-between px-4 cursor-move border-b border-white/10 bg-white/5 ${
          isMaximized() ? 'h-14 md:h-16' : 'h-10 md:h-12'
        }`}
      >
        <div class="flex items-center gap-3">
          <span class={`font-black uppercase tracking-[0.2em] text-white/50 ${
            isMaximized() ? 'text-xs md:text-sm' : 'text-[10px] md:text-xs'
          }`}>
            {props.data.label || props.data.id}
          </span>
        </div>
        <div class="flex items-center gap-2 md:gap-3">
           <button 
             onClick={toggleMaximize}
             class={`tap-target rounded-full transition-all flex items-center justify-center ${
               isMaximized() ? 'bg-cyan-500/20 text-cyan-400' : 'bg-white/5 text-white/40 hover:bg-cyan-500/20 hover:text-cyan-400'
             }`}
           >
             {isMaximized() ? <Minimize2 size={24} /> : <Maximize2 size={20} />}
           </button>
           <button 
             onClick={(e) => { e.stopPropagation(); windowActions.close(props.data.id); }}
             class="tap-target rounded-full bg-white/5 text-white/40 hover:bg-red-500/20 hover:text-red-400 transition-all flex items-center justify-center"
           >
             <X size={24} />
           </button>
        </div>
      </div>

      {/* Content */}
      <div class="flex-1 overflow-auto custom-scrollbar relative">
        {props.children}
      </div>
    </div>
  );

  return (
    <Show when={isMaximized()} fallback={windowContent()}>
        <Portal mount={document.body}>
            {windowContent()}
        </Portal>
    </Show>
  );
};
