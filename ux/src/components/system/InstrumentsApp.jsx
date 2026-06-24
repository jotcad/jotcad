import { createSignal, createMemo, Show, For } from 'solid-js';
import { 
  getInstrumentsConfig, 
  setInstrumentsConfig 
} from '../../lib/vfs/InstrumentsProvider';
import { vfs, mesh, Selector } from '../../lib/VFSManager';
import { meshTopology } from '../../lib/state/MeshState';
import { 
  Music, 
  Sliders, 
  Send, 
  Wifi, 
  WifiOff, 
  Check, 
  AlertTriangle 
} from 'lucide-solid';

export const InstrumentsApp = () => {
  const [jsonText, setJsonText] = createSignal(
    JSON.stringify(getInstrumentsConfig(), null, 2)
  );
  const [statusMessage, setStatusMessage] = createSignal('');
  const [statusType, setStatusType] = createSignal('info'); // 'info' | 'success' | 'error'
  const [selectedInstIndex, setSelectedInstIndex] = createSignal(0);

  // Parse instruments list dynamically from text
  const instrumentsList = createMemo(() => {
    try {
      const parsed = JSON.parse(jsonText());
      return parsed.instruments || [];
    } catch (e) {
      return getInstrumentsConfig().instruments || [];
    }
  });

  const handleSelectInstrument = async (index) => {
    setSelectedInstIndex(index);
    try {
      const sel = new Selector('synth/control', { control: 7, value: index });
      await vfs.readSelector(sel);
      
      const name = instrumentsList()[index]?.name || 'Unknown';
      setStatusMessage(`Switched active instrument to #${index}: ${name}`);
      setStatusType('info');
      setTimeout(() => setStatusMessage(''), 2000);
    } catch (e) {
      console.error('VFS change instrument failed:', e);
      setStatusMessage('Failed to change instrument.');
      setStatusType('error');
    }
  };

  // Validate JSON string on the fly
  const isValidJson = createMemo(() => {
    try {
      JSON.parse(jsonText());
      return true;
    } catch (e) {
      return false;
    }
  });

  // Check if any ESP32 dual-music-synth node is detected in the topology
  const activeSynthPeer = createMemo(() => {
    return meshTopology.peers.find(p => p.id.toLowerCase().includes('synth') || p.id.toLowerCase().includes('esp32-dual'));
  });

  const handleApply = async () => {
    if (!isValidJson()) {
      setStatusMessage('Invalid JSON format. Please check syntax.');
      setStatusType('error');
      return;
    }

    try {
      const parsed = JSON.parse(jsonText());
      setInstrumentsConfig(parsed);
      
      // Notify VFS subscribers (e.g. ESP32 node) of the config publication
      const sel = new Selector('instrument/config');
      await mesh.notify(sel, parsed);

      setStatusMessage('Instruments config saved & published to mesh!');
      setStatusType('success');
      
      setTimeout(() => setStatusMessage(''), 3000);
    } catch (e) {
      console.error('Failed to publish configuration:', e);
      setStatusMessage(`Publish failed: ${e.message}`);
      setStatusType('error');
    }
  };

  // Virtual keyboard key handlers
  const playNote = async (noteNumber) => {
    try {
      const sel = new Selector('synth/note', { note: noteNumber, type: 'on', velocity: 100 });
      await vfs.readSelector(sel);
    } catch (e) {
      console.error('VFS play note failed:', e);
    }
  };

  const stopNote = async (noteNumber) => {
    try {
      const sel = new Selector('synth/note', { note: noteNumber, type: 'off' });
      await vfs.readSelector(sel);
    } catch (e) {
      console.error('VFS stop note failed:', e);
    }
  };

  const KEYS = [
    { note: 60, name: 'C4', isBlack: false },
    { note: 61, name: 'C#4', isBlack: true },
    { note: 62, name: 'D4', isBlack: false },
    { note: 63, name: 'D#4', isBlack: true },
    { note: 64, name: 'E4', isBlack: false },
    { note: 65, name: 'F4', isBlack: false },
    { note: 66, name: 'F#4', isBlack: true },
    { note: 67, name: 'G4', isBlack: false },
    { note: 68, name: 'G#4', isBlack: true },
    { note: 69, name: 'A4', isBlack: false },
    { note: 70, name: 'A#4', isBlack: true },
    { note: 71, name: 'B4', isBlack: false },
    { note: 72, name: 'C5', isBlack: false }
  ];

  return (
    <div class="flex flex-col h-full bg-slate-950/80 backdrop-blur-md text-slate-100 p-4 font-sans select-none overflow-y-auto custom-scrollbar">
      
      {/* Header Panel */}
      <div class="flex items-center justify-between mb-4 border-b border-white/10 pb-3 shrink-0">
        <div class="flex items-center gap-2">
          <Music class="text-cyan-400 animate-pulse" size={20} />
          <span class="text-xs font-black uppercase tracking-widest text-slate-300">Synth Control Panel</span>
        </div>
        
        {/* Connection Status Badge */}
        <div class="flex items-center gap-2 px-3 py-1 rounded-full border bg-black/40 text-[10px] uppercase font-bold tracking-wider transition-all">
          <Show when={activeSynthPeer()} fallback={
            <span class="flex items-center gap-1.5 text-rose-400 border-rose-500/20">
              <WifiOff size={12} />
              Synth Offline
            </span>
          }>
            <span class="flex items-center gap-1.5 text-emerald-400 border-emerald-500/20 animate-pulse">
              <Wifi size={12} />
              Synth Connected ({activeSynthPeer().id.split('-').pop()})
            </span>
          </Show>
        </div>
      </div>

      {/* Active Instrument Selector Card */}
      <div class="mb-4 bg-black/40 border border-white/10 rounded-xl p-3 flex flex-col sm:flex-row sm:items-center justify-between gap-3 shrink-0">
        <div class="flex flex-col gap-0.5">
          <span class="text-[10px] font-black text-white/40 tracking-widest uppercase">Active Preset Voice</span>
          <span class="text-xs font-bold text-cyan-300">Switch active patch remote over mesh (0-127)</span>
        </div>
        <select
          class="bg-slate-900/90 border border-white/10 text-cyan-100 rounded-lg px-3 py-2 text-xs font-bold outline-none focus:border-cyan-500/50 transition-all max-w-xs cursor-pointer custom-scrollbar"
          value={selectedInstIndex()}
          onChange={(e) => handleSelectInstrument(parseInt(e.currentTarget.value, 10))}
        >
          <For each={instrumentsList()}>
            {(inst, idx) => {
              const typeStr = 
                inst.wave === -1 ? 'Sine' : 
                inst.wave === -2 ? 'Triangle' : 
                inst.wave === -3 ? 'Saw' : 
                inst.wave === -4 ? 'Pulse' : 
                inst.wave === -5 ? 'Noise' : 
                inst.wave === 1 ? 'Wavetable' : 
                inst.wave === 2 ? 'Sample' : 
                inst.wave === 3 ? 'Stream' : 
                inst.wave === 4 ? 'FM Custom' : 'Unknown';
              return (
                <option value={idx()}>
                  {idx()}: {inst.name} ({typeStr})
                </option>
              );
            }}
          </For>
        </select>
      </div>

      {/* Main Content Layout */}
      <div class="flex-1 flex flex-col md:flex-row gap-4 min-h-[300px] overflow-hidden">
        
        {/* JSON Editor Box */}
        <div class="flex-1 flex flex-col bg-black/60 border border-white/10 rounded-xl p-3 overflow-hidden">
          <div class="flex items-center justify-between mb-2 shrink-0">
            <span class="text-[10px] font-black text-white/50 tracking-wider uppercase flex items-center gap-1.5">
              <Sliders size={12} />
              Instrument Definitions JSON
            </span>
            <span class={`text-[9px] px-2 py-0.5 rounded font-black tracking-widest ${isValidJson() ? 'bg-emerald-500/20 text-emerald-400' : 'bg-rose-500/20 text-rose-400'}`}>
              {isValidJson() ? 'VALID JSON' : 'SYNTAX ERROR'}
            </span>
          </div>

          <textarea
            class="flex-1 bg-slate-900/60 border border-white/5 focus:border-cyan-500/50 outline-none rounded-lg p-3 font-mono text-[11px] text-cyan-100/90 leading-relaxed resize-none overflow-y-auto custom-scrollbar transition-all"
            value={jsonText()}
            onInput={(e) => setJsonText(e.currentTarget.value)}
            spellcheck={false}
          />
        </div>
      </div>

      {/* Controls & Operations */}
      <div class="mt-4 flex flex-col gap-4 shrink-0">
        <div class="flex items-center justify-between gap-4">
          
          {/* Action Message Indicator */}
          <div class="flex-1 text-[11px] font-bold">
            <Show when={statusMessage()}>
              <div class={`flex items-center gap-2 px-3 py-2 rounded-lg border bg-black/40 ${
                statusType() === 'success' ? 'text-emerald-400 border-emerald-500/30' : 
                statusType() === 'error' ? 'text-rose-400 border-rose-500/30' : 'text-cyan-400 border-cyan-500/30'
              }`}>
                <Show when={statusType() === 'success'} fallback={<AlertTriangle size={14} />}>
                  <Check size={14} />
                </Show>
                {statusMessage()}
              </div>
            </Show>
          </div>

          {/* Apply Button */}
          <button
            onClick={handleApply}
            disabled={!isValidJson()}
            class={`flex items-center gap-2 px-5 py-2.5 rounded-xl font-black uppercase text-xs tracking-wider transition-all border ${
              isValidJson() 
                ? 'bg-gradient-to-r from-cyan-500/20 to-blue-500/20 border-cyan-500/50 text-cyan-300 hover:shadow-[0_0_15px_rgba(6,182,212,0.4)] active:scale-95' 
                : 'bg-slate-900 border-white/5 text-slate-500 cursor-not-allowed'
            }`}
          >
            <Send size={12} />
            Apply & Publish Config
          </button>
        </div>

        {/* Keyboard Header */}
        <div class="border-t border-white/10 pt-4 flex flex-col gap-2">
          <span class="text-[10px] font-black text-white/40 tracking-widest uppercase">Virtual MIDI Test Board</span>
          
          {/* Piano Keys View */}
          <div class="relative flex bg-slate-900 border border-white/5 rounded-xl p-3 h-32 select-none overflow-hidden justify-center">
            <div class="relative flex w-full max-w-md h-full">
              <For each={KEYS}>
                {(key) => {
                  if (key.isBlack) return null;
                  
                  // Find adjacent black key for overlay
                  const blackKey = KEYS.find(k => k.isBlack && k.note === key.note + 1);
                  
                  return (
                    <div class="flex-1 relative group h-full">
                      {/* White Key */}
                      <button
                        onPointerDown={() => playNote(key.note)}
                        onPointerUp={() => stopNote(key.note)}
                        onPointerLeave={() => stopNote(key.note)}
                        class="w-full h-full bg-slate-100 hover:bg-slate-200 active:bg-cyan-200 border-r border-slate-300 rounded-b shadow-md transition-all cursor-pointer relative"
                        title={key.name}
                      >
                        <span class="absolute bottom-1.5 left-1/2 -translate-x-1/2 text-[9px] font-black text-slate-400">
                          {key.name}
                        </span>
                      </button>

                      {/* Overlaid Black Key */}
                      <Show when={blackKey}>
                        <button
                          onPointerDown={(e) => {
                            e.stopPropagation();
                            playNote(blackKey.note);
                          }}
                          onPointerUp={(e) => {
                            e.stopPropagation();
                            stopNote(blackKey.note);
                          }}
                          onPointerLeave={(e) => {
                            e.stopPropagation();
                            stopNote(blackKey.note);
                          }}
                          class="absolute top-0 right-0 translate-x-1/2 w-7 h-3/5 bg-slate-950 hover:bg-slate-800 active:bg-cyan-800 border-b border-l border-r border-white/10 rounded-b shadow-lg z-20 transition-all cursor-pointer"
                          title={blackKey.name}
                        />
                      </Show>
                    </div>
                  );
                }}
              </For>
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
