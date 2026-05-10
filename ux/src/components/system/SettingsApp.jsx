import { createSignal, Show } from 'solid-js';
import { syncStatus, lastSyncTime, cloudAccount } from '../../lib/state/SyncState';
import { Cloud, Network, Shield, Trash2, RefreshCw } from 'lucide-solid';
import { CloudSync } from '../../lib/vfs/CloudSync';
import { blackboard } from '../../lib/blackboard';

export const SettingsApp = () => {
    const [showConnect, setShowConnect] = createSignal(false);
    const handleSync = () => {
        CloudSync.syncAll();
    };

    return (
        <div class="w-full h-full bg-slate-900/50 flex flex-col p-6 md:p-10 gap-10 overflow-y-auto custom-scrollbar">
            
            {/* Cloud Connection Modal */}
            <Show when={showConnect()}>
                <div class="fixed inset-0 z-[2000] flex items-center justify-center p-4 md:p-10 pointer-events-none">
                    <div 
                        class="absolute inset-0 bg-black/80 backdrop-blur-md pointer-events-auto" 
                        onClick={() => setShowConnect(false)} 
                    />
                    <div class="relative w-full max-w-2xl bg-slate-900 border-2 border-cyan-400 rounded-3xl shadow-[0_0_50px_rgba(34,211,238,0.2)] p-8 flex flex-col gap-8 pointer-events-auto">
                        <div class="flex justify-between items-center">
                            <div class="flex items-center gap-4 text-cyan-400">
                                <Cloud size={32} />
                                <h2 class="text-xl font-black uppercase tracking-widest">Connect Cloud Storage</h2>
                            </div>
                            <button 
                                onClick={() => setShowConnect(false)}
                                class="tap-target p-2 rounded-full hover:bg-white/10"
                            >
                                <RefreshCw size={24} class="rotate-45" />
                            </button>
                        </div>

                        <div class="text-white/60 text-readable">
                            JotCAD uses the <strong>RemoteStorage</strong> protocol to sync your scripts and workspace across devices without a central server. Choose a provider like Google Drive or Dropbox to begin.
                        </div>

                        <div 
                            id="rs-widget-container" 
                            class="flex items-center justify-center min-h-[100px]"
                        />

                        <button 
                            onClick={() => setShowConnect(false)}
                            class="w-full py-4 rounded-xl bg-cyan-500/20 text-cyan-400 border border-cyan-400/30 font-black uppercase tracking-widest hover:bg-cyan-500 hover:text-white transition-all"
                        >
                            Return to Settings
                        </button>
                    </div>
                </div>
            </Show>

            {/* Cloud Storage Section */}
            <section class="flex flex-col gap-6">
                <div class="flex items-center gap-3 text-cyan-400">
                    <Cloud size={24} />
                    <h2 class="text-ui-label font-black uppercase tracking-[0.2em]">Cloud Persistence</h2>
                </div>
                
                <div class="bg-black/60 border border-white/10 rounded-2xl p-6 flex flex-col gap-6 shadow-xl">
                    <div class="flex flex-col md:flex-row md:items-center justify-between gap-4">
                        <div class="flex flex-col gap-1">
                            <span class="text-ui-label text-white/40 uppercase font-bold tracking-widest">Account Status</span>
                            <span class="text-readable font-mono text-white/90">
                                {cloudAccount() ? cloudAccount().email : 'Not Connected'}
                            </span>
                        </div>
                        <button 
                            onClick={() => setShowConnect(true)}
                            class="tap-target px-6 py-3 rounded-xl bg-white/5 border border-white/10 text-white/80 hover:bg-cyan-500/20 hover:text-cyan-400 hover:border-cyan-400/50 transition-all text-ui-label font-black uppercase tracking-widest flex items-center justify-center gap-3 shadow-lg"
                        >
                            <Cloud size={18} />
                            Manage Connection
                        </button>
                    </div>

                    <div class="flex flex-col md:flex-row md:items-center justify-between border-t border-white/10 pt-6 gap-4">
                        <div class="flex flex-col gap-1">
                            <span class="text-ui-label text-white/40 uppercase font-bold tracking-widest">Last Synchronization</span>
                            <span class="text-readable font-mono text-cyan-200/80">
                                {new Date(lastSyncTime()).toLocaleString()}
                            </span>
                        </div>
                        <button 
                            onClick={handleSync}
                            disabled={syncStatus() === 'syncing'}
                            class="tap-target px-6 rounded-xl bg-cyan-500/10 border border-cyan-500/30 text-cyan-400 hover:bg-cyan-500/20 transition-all text-ui-label font-black uppercase tracking-widest flex items-center justify-center gap-3 shadow-lg"
                        >
                            <RefreshCw size={18} class={syncStatus() === 'syncing' ? 'animate-spin' : ''} />
                            Sync Now
                        </button>
                    </div>
                </div>
            </section>

            {/* Mesh Network Section */}
            <section class="flex flex-col gap-6">
                <div class="flex items-center gap-3 text-cyan-400">
                    <Network size={24} />
                    <h2 class="text-ui-label font-black uppercase tracking-[0.2em]">Mesh Connectivity</h2>
                </div>
                
                <div class="bg-black/60 border border-white/10 rounded-2xl p-6 flex flex-col gap-6 shadow-xl">
                     <div class="flex flex-col md:flex-row md:items-center justify-between gap-2">
                        <span class="text-ui-label text-white/40 uppercase font-bold tracking-widest">Local Peer ID</span>
                        <span class="text-readable font-mono text-white/80 bg-white/5 px-3 py-2 rounded-lg truncate max-w-full md:max-w-[300px] border border-white/5">
                            {blackboard.peerId}
                        </span>
                     </div>
                     <div class="flex items-center justify-between border-t border-white/10 pt-6">
                        <span class="text-ui-label text-white/40 uppercase font-bold tracking-widest">Mesh Status</span>
                        <div class="flex items-center gap-3">
                             <div class={`w-3 h-3 rounded-full ${blackboard.isConnected() ? 'bg-green-500 shadow-[0_0_12px_rgba(34,197,94,0.7)]' : 'bg-red-500'}`} />
                             <span class="text-readable font-black text-white/90 uppercase tracking-widest">
                                 {blackboard.isConnected() ? 'ONLINE' : 'OFFLINE'}
                             </span>
                        </div>
                     </div>
                </div>
            </section>

            {/* Danger Zone */}
            <section class="flex flex-col gap-6 mt-auto">
                <div class="flex items-center gap-3 text-cyan-400">
                    <Shield size={24} />
                    <h2 class="text-ui-label font-black uppercase tracking-[0.2em]">System Recovery</h2>
                </div>
                
                <div class="bg-cyan-500/10 border border-cyan-500/30 rounded-2xl p-6 shadow-xl">
                    <button 
                        onClick={() => { if(confirm('Clear all local data and reset?')) blackboard.clearStorage(); }}
                        class="w-full tap-target rounded-xl bg-cyan-500/20 border border-cyan-500/40 text-cyan-400 hover:bg-cyan-500 hover:text-white transition-all text-ui-label font-black uppercase tracking-widest flex items-center justify-center gap-3 shadow-lg"
                    >
                        <Trash2 size={20} />
                        Factory Reset local data
                    </button>
                </div>
            </section>

        </div>
    );
};
