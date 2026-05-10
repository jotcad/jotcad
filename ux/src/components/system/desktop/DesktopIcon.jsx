import { createSignal, onMount, createMemo, Show } from 'solid-js';
import interact from 'interactjs';
import * as Icons from 'lucide-solid';
import { blackboard } from '../../../lib/blackboard';
import { desktopActions, windowActions, isConnected } from '../../../lib/state/AppState';
import { syncStatus } from '../../../lib/state/SyncState';

export const DesktopIcon = (props) => {
  let iconRef;
  const [isPressed, setIsPressed] = createSignal(false);
  const [longPressTimer, setLongPressTimer] = createSignal(null);

  const IconComponent = () => {
    if (props.data.type === 'folder') return Icons.Folder;
    const iconName = props.data.icon || 'Box';
    const Icon = Icons[iconName] || Icons.Box;
    if (!Icons[iconName]) {
        console.warn(`[DesktopIcon] Lucide icon not found: ${iconName}. Falling back to Box.`);
    }
    return Icon;
  };

  onMount(() => {
    console.log(`[DesktopIcon] Mounting icon: ${props.data.id} at ${props.data.x}, ${props.data.y}`);
    if (props.isNested) return;

    interact(iconRef).draggable({
      listeners: {
        move(event) {
          const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
          const newX = props.data.x + event.dx / s;
          const newY = props.data.y + event.dy / s;

          if (props.isUserOp) {
             blackboard.updateEditorState(props.data.target, {
                 schema: {
                     ...blackboard.dynamicOps()[props.data.target]?.schema,
                     _desktopPos: { x: newX, y: newY }
                 }
             });
          } else {
            desktopActions.updateIcon(props.data.id, { x: newX, y: newY });
          }
        }
      }
    });
  });

  const handlePointerDown = (e) => {
    setIsPressed(true);
    const timer = setTimeout(() => {
      if (isPressed()) {
        blackboard.emit('desktop:longpress', { id: props.data.id });
      }
    }, 600);
    setLongPressTimer(timer);
  };

  const handlePointerUp = () => {
    if (longPressTimer()) clearTimeout(longPressTimer());
    setIsPressed(false);
  };

  const handleClick = () => {
    if (props.data.type === 'folder') {
      windowActions.open('folder', props.data.id, { label: props.data.label, children: props.data.children });
    } else if (props.data.type === 'app') {
      windowActions.open(props.data.target, props.data.id, { label: props.data.label });
    } else if (props.data.type === 'editor') {
       blackboard.openOp(props.data.target);
    } else if (props.data.type === 'action') {
        if (props.data.target === 'new_op') {
            blackboard.createNewOp();
        }
    }
  };

  return (
    <div
      ref={iconRef}
      onPointerDown={handlePointerDown}
      onPointerUp={handlePointerUp}
      onClick={handleClick}
      class={`${props.isNested ? 'relative' : 'absolute'} flex flex-col items-center gap-2 group cursor-pointer select-none z-[20]`}
      style={{
        left: props.isNested ? 'auto' : `${props.data.x}px`,
        top: props.isNested ? 'auto' : `${props.data.y}px`,
        width: '100px'
      }}
    >
      <div class={`relative w-16 h-16 md:w-14 md:h-14 rounded-2xl md:rounded-xl flex items-center justify-center transition-all ${
        isPressed() ? 'scale-90 bg-white/30' : 'bg-black/60 md:bg-white/10 group-hover:bg-cyan-500/20'
      } border-2 ${props.isUserOp ? 'border-cyan-400 shadow-[0_0_15px_rgba(34,211,238,0.4)]' : 'border-white/10'} group-hover:border-cyan-400/50 shadow-xl`}>
        
        {/* Status Indicators */}
        <Show when={props.data.target === 'mesh'}>
           <div class={`absolute -top-1 -right-1 w-4 h-4 md:w-3 md:h-3 rounded-full border-2 border-black ${isConnected() ? 'bg-green-500 shadow-[0_0_12px_rgba(34,197,94,0.7)]' : 'bg-red-500'}`} />
        </Show>

        <Show when={props.data.target === 'settings'}>
            <div class="absolute -top-1 -right-1">
                <Show when={syncStatus() === 'syncing'}>
                    <Icons.RefreshCw size={16} class="text-cyan-400 animate-spin" />
                </Show>
                <Show when={syncStatus() === 'conflict'}>
                    <Icons.AlertCircle size={18} class="text-orange-500 bg-black rounded-full shadow-lg" />
                </Show>
                <Show when={syncStatus() === 'error'}>
                    <Icons.CloudOff size={18} class="text-red-500 bg-black rounded-full shadow-lg" />
                </Show>
            </div>
        </Show>

        <div class={`${
            props.data.type === 'folder' ? 'text-amber-400/90' : 
            props.data.type === 'editor' ? 'text-cyan-400/90' :
            'text-white/90'
        } group-hover:text-cyan-300 transition-colors`}>
           <IconComponent size={32} />
        </div>
      </div>
      <span class="text-[12px] md:text-[10px] font-black text-white/70 group-hover:text-cyan-200 text-center truncate w-full px-1 drop-shadow-[0_2px_4px_rgba(0,0,0,0.8)] uppercase tracking-wider">
        {props.data.label}
      </span>
    </div>
  );
};
