import { For } from 'solid-js';
import {
  Activity,
  CheckCircle2,
  Check,
  FileJson
} from 'lucide-solid';

/**
 * A list of status pips laid out contiguously in a clockwise direction along the border.
 */
export const StatusBadge = (props) => {
  // 1. Gather active pips
  const badges = [];

  // Grey: Schema
  if (props.states.includes('SCHEMA')) {
    badges.push({
      id: 'schema',
      color: 'bg-slate-600',
      title: 'Schema Declared',
      icon: <FileJson size={10} class="text-white" />,
    });
  }

  // White/Yellow/Blue: Availability State
  if (props.states.includes('AVAILABLE')) {
    badges.push({
      id: 'available',
      color: 'bg-white',
      borderColor: 'border-node',
      title: 'Available',
      icon: <Check size={10} class="text-node stroke-[4]" />,
    });
  } else if (props.states.includes('PROVISIONING')) {
    badges.push({
      id: 'provisioning',
      color: 'bg-provisioning',
      title: 'Provisioning',
      animate: 'animate-pulse',
    });
  } else if (props.states.includes('PENDING')) {
    badges.push({
      id: 'pending',
      color: 'bg-pending',
      title: 'Pending',
      animate: 'animate-bounce',
    });
  }

  // Blue/Cyan: Thumbnail
  if (props.hasThumbnail) {
    badges.push({
      id: 'thumb-ready',
      color: 'bg-blue-600',
      title: 'Thumbnail Ready',
      icon: <CheckCircle2 size={10} class="text-white" />,
    });
  } else if (props.isGenerating) {
    badges.push({
      id: 'thumb-gen',
      color: 'bg-cyan-500',
      title: 'Generating Thumbnail...',
      animate: 'animate-pulse',
      icon: <Activity size={10} class="text-white" />,
    });
  }

  // White: Result Count
  if (props.resultCount > 1) {
    badges.push({
      id: 'count',
      color: 'bg-white',
      title: `${props.resultCount} results`,
      content: (
        <span class="text-blackboard text-[8px] font-black">
          {props.resultCount}
        </span>
      ),
    });
  }

  // 2. Define clockwise positions (Top -> Right -> Bottom -> Left)
  // Offset from the chip edge (-6px means slightly overlapping the border)
  const positions = [
    { top: '-6px', left: '-6px' }, // Pos 0: Top-Left
    { top: '-6px', left: '14px' }, // Pos 1
    { top: '-6px', left: '34px' }, // Pos 2
    { top: '-6px', left: '54px' }, // Pos 3: Top-Right
    { top: '14px', left: '54px' }, // Pos 4
    { top: '34px', left: '54px' }, // Pos 5
    { top: '54px', left: '54px' }, // Pos 6: Bottom-Right
    { top: '54px', left: '34px' }, // Pos 7
    { top: '54px', left: '14px' }, // Pos 8
    { top: '54px', left: '-6px' }, // Pos 9: Bottom-Left
  ];

  return (
    <For each={badges}>
      {(badge, i) => (
        <div
          class={`absolute w-4 h-4 rounded-full border border-blackboard flex items-center justify-center shadow-lg z-20 transition-all duration-300 ${
            badge.color
          } ${badge.borderColor || ''} ${badge.animate || ''}`}
          style={{
            top: positions[i()]?.top || '0px',
            left: positions[i()]?.left || '0px',
          }}
          title={badge.title}
        >
          {badge.icon || badge.content}
        </div>
      )}
    </For>
  );
};
