import { createSignal, onMount, Show, createEffect, onCleanup, createMemo } from 'solid-js';
import interact from 'interactjs';
import { X, Maximize2, Minimize2, Minus } from 'lucide-solid';
import { windowActions } from '../../../lib/state/DesktopState';
import { blackboard } from '../../../lib/blackboard';

export const Window = (props) => {
  let winRef;
  const [isInteracting, setIsInteracting] = createSignal(false);
  const [screenSize, setScreenSize] = createSignal({ w: window.innerWidth, h: window.innerHeight });

  // Reactive derived signal from store
  const isMaximized = () => props.data.isMaximized || false;
  const view = () => window._JOT_VIEW ? window._JOT_VIEW() : { x: 0, y: 0, scale: 1 };

  onMount(() => {
    const handleResize = () => setScreenSize({ w: window.innerWidth, h: window.innerHeight });
    window.addEventListener('resize', handleResize);
    onCleanup(() => window.removeEventListener('resize', handleResize));

    interact(winRef)
      .draggable({
        allowFrom: '.win-header',
        listeners: {
          start(event) { 
            setIsInteracting(true); 
            windowActions.raise(props.data.id);
            if (event.pointerId !== undefined && event.target && event.target.setPointerCapture) {
                event.target.setPointerCapture(event.pointerId);
            }
          },
          move(event) {
            if (isMaximized()) return;
            const s = view().scale;
            windowActions.update(props.data.id, {
              pos: { 
                x: props.data.pos.x + event.dx / s, 
                y: props.data.pos.y + event.dy / s 
              }
            });
          },
          end() { setIsInteracting(false); }
        }
      })
      .resizable({
        edges: { left: true, right: true, bottom: true, top: true },
        margin: 12, 
        listeners: {
          start(event) { 
            setIsInteracting(true); 
            windowActions.raise(props.data.id);
            if (event.pointerId !== undefined && event.target && event.target.setPointerCapture) {
                event.target.setPointerCapture(event.pointerId);
            }
          },
          move(event) {
            if (isMaximized()) return;
            const s = view().scale;
            windowActions.update(props.data.id, {
              size: {
                width: event.rect.width / s,
                height: event.rect.height / s
              },
              pos: {
                x: props.data.pos.x + event.deltaRect.left / s,
                y: props.data.pos.y + event.deltaRect.top / s
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

  // Memoized style to ensure reactive recalculation during zoom/pan
  const windowStyle = createMemo(() => {
    const v = view();
    const s = v.scale;
    const isMax = isMaximized();
    
    if (isMax) {
        return {
            left: `${-v.x / s}px`,
            top: `${-v.y / s}px`,
            width: `${screenSize().w / s}px`,
            height: `${screenSize().h / s}px`,
            'z-index': 5000 + (props.data.zIndex || 0),
            'border-radius': '0',
            'border-width': '0'
        };
    }

    return {
        left: `${props.data.pos.x}px`,
        top: `${props.data.pos.y}px`,
        width: `${props.data.size.width}px`,
        height: `${props.data.size.height}px`,
        'z-index': props.data.zIndex || 10,
        'border-radius': '1rem',
        'border-width': '2px'
    };
  });

  return (
    <div
      ref={winRef}
      onPointerDown={() => { windowActions.raise(props.data.id); }}
      class={`jot-window absolute pointer-events-auto border-cyan-400 bg-black/95 backdrop-blur-3xl shadow-2xl flex flex-col overflow-hidden touch-none ${
        isMaximized() ? 'is-maximized' : 
        isInteracting() ? 'border-cyan-300 ring-4 ring-cyan-400/20' : 'transition-all duration-200'
      }`}
      style={windowStyle()}
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
             class="tap-target rounded-full bg-white/5 text-white/40 hover:bg-cyan-500/20 hover:text-cyan-400 transition-all flex items-center justify-center"
           >
             <X size={24} />
           </button>
        </div>
      </div>

      {/* Content */}
      <div class="flex-1 overflow-auto custom-scrollbar relative">
        {props.children}
        {/* Spacer for resize handle */}
        <Show when={!isMaximized()}>
            <div class="h-8 shrink-0 pointer-events-none" />
        </Show>
      </div>

      {/* Resize Handle (Bottom-Right) */}
      <Show when={!isMaximized()}>
          <div 
            class="absolute bottom-0 right-0 w-8 h-8 cursor-nwse-resize z-[1001] flex items-end justify-end p-1.5 group/resize pointer-events-auto touch-none"
          >
              <div class="w-2 h-2 rounded-full bg-cyan-400/20 group-hover/resize:bg-cyan-400 transition-colors shadow-[0_0_8px_rgba(34,211,238,0.4)]" />
          </div>
      </Show>
    </div>
  );
};
