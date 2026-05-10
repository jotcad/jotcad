import { Show } from 'solid-js';
import { syncStatus, lastSyncTime, cloudAccount } from '../../lib/state/SyncState';
import { Cloud, CloudOff, RefreshCw, AlertCircle } from 'lucide-solid';
import { CloudSync } from '../../lib/vfs/CloudSync';

export const SyncBadge = () => {
    const handleSync = () => {
        CloudSync.syncAll();
    };

    return (
        <div class="flex items-center gap-2 px-3 py-1.5 rounded-full bg-black/80 backdrop-blur-md border-2 border-cyan-400 shadow-xl pointer-events-auto select-none">
            <button 
                onClick={handleSync}
                disabled={syncStatus() === 'syncing'}
                class="flex items-center gap-2 group"
                title={cloudAccount() ? `Syncing with ${cloudAccount().email}` : 'Connect Cloud Storage'}
            >
                <div class="relative">
                    <Show when={syncStatus() === 'idle'}>
                        <Cloud size={16} class="text-white/40 group-hover:text-cyan-400 transition-colors" />
                    </Show>
                    <Show when={syncStatus() === 'syncing'}>
                        <RefreshCw size={16} class="text-cyan-400 animate-spin" />
                    </Show>
                    <Show when={syncStatus() === 'conflict'}>
                        <div class="relative">
                           <Cloud size={16} class="text-orange-400" />
                           <AlertCircle size={8} class="absolute -top-1 -right-1 text-red-500 bg-black rounded-full" />
                        </div>
                    </Show>
                    <Show when={syncStatus() === 'error'}>
                        <CloudOff size={16} class="text-red-500" />
                    </Show>
                </div>
                
                <div class="flex flex-col">
                    <span class="text-[9px] font-black uppercase tracking-tighter text-white/70">
                        {syncStatus() === 'syncing' ? 'Syncing...' : 
                         syncStatus() === 'conflict' ? 'Conflict' :
                         syncStatus() === 'error' ? 'Sync Failed' : 'Cloud Sync'}
                    </span>
                    <Show when={syncStatus() === 'idle'}>
                        <span class="text-[7px] font-mono text-white/20">
                            Last: {new Date(lastSyncTime()).toLocaleTimeString()}
                        </span>
                    </Show>
                </div>
            </button>
        </div>
    );
};
