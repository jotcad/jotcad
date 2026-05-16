import { onMount, Show } from 'solid-js';
import interact from 'interactjs';
import { Cpu } from 'lucide-solid';
import { blackboard } from '../../lib/blackboard';

/**
 * An Agent bubble (Hexagonal Worker).
 */
export const AgentNode = (props) => {
  let nodeRef;
  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          if (blackboard.gestureOwner() === 'blackboard') return;
          props.onMove(props.data.cid, event.dx, event.dy);
        },
      },
    });
  });

  return (
    <div
      ref={nodeRef}
      data-id={props.data.cid}
      class={`agent-node absolute w-14 h-14 flex items-center justify-center cursor-move z-20 group`}
      style={{
        left: `${props.pos?.x || 0}px`,
        top: `${props.pos?.y || 0}px`,
        'pointer-events': 'auto',
      }}
    >
      <svg
        viewBox="0 0 100 100"
        class="absolute inset-0 w-full h-full drop-shadow-2xl"
      >
        <polygon
          points="28,8 72,8 94,50 72,92 28,92 6,50"
          fill="#f97316"
          stroke="rgba(255,255,255,0.2)"
          stroke-width="4"
          class="group-hover:fill-orange-400 transition-colors"
        />
      </svg>

      <Cpu size={20} class="text-blackboard relative z-10" />

      <div class="absolute top-full mt-2 flex flex-col items-center gap-0.5 z-10 pointer-events-none">
        <div class="px-1.5 py-0.5 rounded text-[7px] font-black bg-black/80 backdrop-blur-sm text-capability whitespace-nowrap border border-capability/20 shadow-lg">
          {props.data.path}
        </div>
        <Show when={props.data.source}>
          <div class="text-[5px] opacity-40 font-mono tracking-tighter text-white uppercase">
            {props.data.source}
          </div>
        </Show>
      </div>
    </div>
  );
};
