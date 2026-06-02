import { createSignal, onMount } from 'solid-js';
import interact from 'interactjs';

/**
 * IconShell: The 'Sensible Base' for all desktop elements.
 * Handles:
 * 1. Draggability (via interactjs)
 * 2. Spatial positioning (absolute)
 * 3. Base styling (glassmorphism container)
 * 4. Interaction states (pressed, hover)
 */
export const IconShell = (props) => {
  let shellRef;
  const [isPressed, setIsPressed] = createSignal(false);

  onMount(() => {
    if (props.isNested) return;

    interact(shellRef).draggable({
      listeners: {
        move(event) {
          const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
          const newX = props.x + event.dx / s;
          const newY = props.y + event.dy / s;
          
          if (props.onMove) {
              props.onMove(newX, newY);
          }
        }
      }
    });
  });

  const handlePointerDown = () => setIsPressed(true);
  const handlePointerUp = () => setIsPressed(false);

  return (
    <div
      ref={shellRef}
      onPointerDown={handlePointerDown}
      onPointerUp={handlePointerUp}
      onClick={() => props.onClick?.()}
      class={`${props.isNested ? 'relative' : 'absolute'} flex flex-col items-center gap-2 group cursor-pointer select-none z-[20]`}
      style={{
        left: props.isNested ? 'auto' : `${props.x}px`,
        top: props.isNested ? 'auto' : `${props.y}px`,
        width: '100px'
      }}
    >
      <div class={`relative w-16 h-16 md:w-14 md:h-14 rounded-2xl md:rounded-xl flex items-center justify-center transition-all ${
        isPressed() ? 'scale-90 bg-white/30' : 'bg-black/60 md:bg-white/10 group-hover:bg-cyan-500/20'
      } border-2 ${props.borderColor || 'border-cyan-400'} shadow-xl ${props.class || ''}`}>
        
        {props.children}

        {/* Dynamic Badge Slot */}
        {props.badge}
      </div>
      
      <span class={`text-[12px] md:text-[10px] font-black text-white/70 group-hover:text-cyan-200 text-center w-full px-1 drop-shadow-[0_2px_4px_rgba(0,0,0,0.8)] tracking-wider break-words leading-tight uppercase`}>
        {props.label}
      </span>
    </div>
  );
};
