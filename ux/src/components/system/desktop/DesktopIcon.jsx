import { createSignal, createMemo, Show } from 'solid-js';
import { 
  Folder, 
  Cloud, 
  Database, 
  Network, 
  Settings, 
  Terminal, 
  Plus, 
  RefreshCw, 
  AlertCircle, 
  CloudOff, 
  FileText, 
  Box 
} from 'lucide-solid';
import { IconShell } from './IconShell';
import { blackboard } from '../../../lib/blackboard';
import { desktopActions, windowActions } from '../../../lib/state/DesktopState';
import { isConnected } from '../../../lib/state/MeshState';
import { syncStatus, isDirty } from '../../../lib/state/SyncState';
import { CloudSync } from '../../../lib/vfs/CloudSync';

const ICON_MAP = {
    Folder,
    Cloud,
    Database,
    Network,
    Settings,
    Terminal,
    Plus,
    RefreshCw,
    AlertCircle,
    CloudOff,
    FileText,
    Box
};

export const DesktopIcon = (props) => {
  const IconComponent = () => {
    if (props.data.type === 'folder') return Folder;
    if (props.data.target === 'sync_cloud') return Cloud;

    const iconName = props.data.icon || 'Box';
    const Icon = ICON_MAP[iconName] || Box;
    return Icon;
  };

  const handleMove = (newX, newY) => {
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
        } else if (props.data.target === 'sync_cloud') {
            CloudSync.syncAll();
        }
    }
  };

  // Construct the badges
  const Badge = () => (
    <>
        <Show when={props.data.target === 'mesh'}>
           <div class={`absolute -top-1 -right-1 w-4 h-4 md:w-3 md:h-3 rounded-full border-2 border-black ${isConnected() ? 'bg-green-500 shadow-[0_0_12px_rgba(34,197,94,0.7)]' : 'bg-red-500'}`} />
        </Show>

        <Show when={props.data.target === 'sync_cloud'}>
           <div class={`absolute -top-1 -right-1 w-4 h-4 md:w-3 md:h-3 rounded-full border-2 border-black ${
             isDirty() ? 'bg-red-500 shadow-[0_0_12px_rgba(239,68,68,0.7)]' : 
             (syncStatus() === 'idle' ? 'bg-green-500 shadow-[0_0_12px_rgba(34,197,94,0.7)]' : 'bg-white/20')
           }`} />
        </Show>

        <Show when={props.data.target === 'settings'}>
            <div class="absolute -top-1 -right-1">
                <Show when={syncStatus() === 'syncing'}>
                    <RefreshCw size={16} class="text-cyan-400 animate-spin" />
                </Show>
                <Show when={syncStatus() === 'conflict'}>
                    <AlertCircle size={18} class="text-orange-500 bg-black rounded-full shadow-lg" />
                </Show>
                <Show when={syncStatus() === 'error'}>
                    <CloudOff size={18} class="text-red-500 bg-black rounded-full shadow-lg" />
                </Show>
            </div>
        </Show>
    </>
  );

  return (
    <IconShell
        x={props.data.x}
        y={props.data.y}
        label={props.data.label}
        isNested={props.isNested}
        borderColor={props.isUserOp ? 'border-cyan-400 shadow-[0_0_15px_rgba(34,211,238,0.4)]' : 'border-cyan-400 group-hover:border-cyan-300'}
        onMove={handleMove}
        onClick={handleClick}
        badge={<Badge />}
    >
        <div class={`${
            props.data.type === 'folder' ? 'text-amber-400/90' : 
            props.data.type === 'editor' ? 'text-cyan-400/90' :
            (props.data.target === 'sync_cloud' && syncStatus() === 'syncing') ? 'text-cyan-400 animate-spin' :
            'text-white/90'
        } group-hover:text-cyan-300 transition-colors`}>
           <IconComponent size={32} />
        </div>
    </IconShell>
  );
};
