import { createSignal, onMount, Show, createEffect, onCleanup, createMemo } from 'solid-js';
import interact from 'interactjs';
import { X, Maximize2, Minimize2, Minus, Trash2, Globe } from 'lucide-solid';
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

  const isUnpublished = createMemo(() => {
    if (props.data.type !== 'editor') return false;
    const path = props.data.opName || '';
    const code = props.data.code || '';
    if (!path) return true;

    const published = blackboard.dynamicOps()[path];
    if (!published) return true;
    
    // Compare normalized scripts to avoid whitespace-only false positives
    const local = code.trim();
    const remote = (published.script || '').trim();
    return local !== remote;
  });

  const handlePublish = (e) => {
    e.stopPropagation();
    let path = (props.data.opName || '').trim();
    
    if (!path) {
        alert("Please enter a name for your operator before publishing.");
        return;
    }

    if (path && !path.includes('/')) path = `user/${path}`;
    
    if (!path.startsWith('user/')) {
        alert("Please enter a valid operator path (e.g. 'user/MyOperator')");
        return;
    }

    // Force immediate store sync for the prefixed path and unify IDs
    windowActions.update(props.data.id, { opName: path, label: path });
    
    if (props.data.id.startsWith('new-op-')) {
        blackboard.rename(props.data.id, path);
    }

    const schema = {
      path,
      arguments: (props.data.args || []).map(arg => ({
        name: arg.name,
        type: arg.type,
        default: arg.testValue
      })),
      outputs: { "$out": { type: "shape" } }
    };
    blackboard.publishDynamicOp(path, schema, props.data.code);
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
        left: `${props.data.pos?.x ?? 100}px`,
        top: `${props.data.pos?.y ?? 100}px`,
        width: `${props.data.size?.width ?? 500}px`,
        height: `${props.data.size?.height ?? 600}px`,
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
          <Show when={props.data.type === 'editor'} fallback={
            <span class={`font-black tracking-[0.2em] text-white/50 ${
                isMaximized() ? 'text-xs md:text-sm' : 'text-[10px] md:text-xs'
            }`}>
                {props.data.label || props.data.id}
            </span>
          }>
             <input 
               class="bg-transparent border-none text-ui-label font-black tracking-tighter text-cyan-400 focus:outline-none w-48 placeholder:text-cyan-400/20"
               value={props.data.opName ?? ''}
               placeholder="Name your op..."
               onPointerDown={e => e.stopPropagation()}
               onInput={e => {
                   const val = e.target.value;
                   windowActions.update(props.data.id, { opName: val, label: val || 'New Operator' });
               }}
               onKeyDown={e => {
                   if (e.key === 'Enter') {
                       e.currentTarget.blur();
                   }
               }}
             />
          </Show>
        </div>
        <div class="flex items-center gap-2 md:gap-3">
           <Show when={props.data.type === 'editor'}>
               <button 
                 onClick={handlePublish}
                 class={`relative tap-target rounded-full bg-white/5 transition-all flex items-center justify-center p-1.5 ${
                    isUnpublished() ? 'text-cyan-400' : 'text-white/20'
                 }`}
                 title="Publish to Mesh"
               >
                 <Globe size={20} />
                 {/* Status Dot */}
                 <div class={`absolute -top-0.5 -right-0.5 w-2.5 h-2.5 rounded-full border-2 border-black ${
                     isUnpublished() ? 'bg-red-500 shadow-[0_0_8px_rgba(239,68,68,0.6)]' : 'bg-green-500 shadow-[0_0_8px_rgba(34,197,94,0.6)]'
                 }`} />
               </button>
               <button 
                 onClick={(e) => {
                     e.stopPropagation();
                     const name = props.data.opName || props.data.id;
                     if (confirm(`Delete operator '${name}' from library?`)) {
                         blackboard.removeDynamicOp(name);
                         windowActions.close(props.data.id);
                     }
                 }}
                 class="tap-target rounded-full bg-white/5 text-white/40 hover:bg-red-500/20 hover:text-red-400 transition-all flex items-center justify-center p-1.5"
                 title="Delete Operator"
               >
                 <Trash2 size={20} />
               </button>
           </Show>
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
