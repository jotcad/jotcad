import { createSignal, onMount, onCleanup, createMemo, For } from 'solid-js';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { meshTopology } from '../../../lib/state/MeshState';

export const RgbLedWindow = (props) => {
  const [r, setR] = createSignal(0);
  const [g, setG] = createSignal(0);
  const [b, setB] = createSignal(0);
  const [isUpdating, setIsUpdating] = createSignal(false);

  // Check connection status based on active mesh peers
  const isConnected = createMemo(() => {
    return meshTopology.peers.some(p => 
      p.id.toLowerCase().includes('rgb') || 
      p.id.toLowerCase().includes('led')
    );
  });

  // Throttled notification helper to avoid overloading mesh connections on dragging
  let lastSentTime = 0;
  let pendingTimeout = null;

  const publishColorThrottled = (red, green, blue) => {
    const now = Date.now();
    if (pendingTimeout) clearTimeout(pendingTimeout);

    const performPublish = async () => {
      lastSentTime = Date.now();
      try {
        await mesh.notify(new Selector('control/rgb'), { r: red, g: green, b: blue });
      } catch (err) {
        console.error('[RgbLedWindow] Mesh notify failed:', err);
      }
    };

    if (now - lastSentTime >= 50) {
      performPublish();
    } else {
      pendingTimeout = setTimeout(performPublish, 50 - (now - lastSentTime));
    }
  };

  const handleSliderChange = (channel, val) => {
    const intVal = parseInt(val, 10);
    if (channel === 'r') setR(intVal);
    else if (channel === 'g') setG(intVal);
    else if (channel === 'b') setB(intVal);

    publishColorThrottled(r(), g(), b());
  };

  const applyPreset = (red, green, blue) => {
    setR(red);
    setG(green);
    setB(blue);
    
    // Presets publish immediately
    if (pendingTimeout) clearTimeout(pendingTimeout);
    mesh.notify(new Selector('control/rgb'), { r: red, g: green, b: blue }).catch(err => {
      console.error('[RgbLedWindow] Preset apply failed:', err);
    });
  };

  onMount(() => {
    // 1. Listen for mesh notifications to sync controls if color is modified externally
    const handleNotify = (selector, payload) => {
      if (selector.path === 'control/rgb') {
        const red = typeof payload.r !== 'undefined' ? payload.r : (payload.red || 0);
        const green = typeof payload.g !== 'undefined' ? payload.g : (payload.green || 0);
        const blue = typeof payload.b !== 'undefined' ? payload.b : (payload.blue || 0);

        setR(red);
        setG(green);
        setB(blue);
      }
    };

    vfs.events.on('notify', handleNotify);

    // 2. Fetch initial state
    const rgbSel = new Selector('control/rgb');
    vfsActions.readSelectorData(rgbSel)
      .then(data => {
        if (data) {
          setR(typeof data.r !== 'undefined' ? data.r : (data.red || 0));
          setG(typeof data.g !== 'undefined' ? data.g : (data.green || 0));
          setB(typeof data.b !== 'undefined' ? data.b : (data.blue || 0));
        }
      })
      .catch(() => {
        // Initial fetch ignored if node wasn't previously cached
      });

    onCleanup(() => {
      vfs.events.off('notify', handleNotify);
      if (pendingTimeout) clearTimeout(pendingTimeout);
    });
  });

  const presets = [
    { name: 'Off', r: 0, g: 0, b: 0, bgClass: 'bg-black border-white/20 text-white/60' },
    { name: 'Red', r: 255, g: 0, b: 0, bgClass: 'bg-red-600/30 border-red-500/50 text-red-200' },
    { name: 'Green', r: 0, g: 255, b: 0, bgClass: 'bg-green-600/30 border-green-500/50 text-green-200' },
    { name: 'Blue', r: 0, g: 0, b: 255, bgClass: 'bg-blue-600/30 border-blue-500/50 text-blue-200' },
    { name: 'Yellow', r: 255, g: 255, b: 0, bgClass: 'bg-yellow-600/30 border-yellow-500/50 text-yellow-200' },
    { name: 'Cyan', r: 0, g: 255, b: 255, bgClass: 'bg-cyan-600/30 border-cyan-500/50 text-cyan-200' },
    { name: 'Magenta', r: 255, g: 0, b: 255, bgClass: 'bg-fuchsia-600/30 border-fuchsia-500/50 text-fuchsia-200' },
    { name: 'White', r: 255, g: 255, b: 255, bgClass: 'bg-white/20 border-white/40 text-white' }
  ];

  return (
    <div class="w-full h-full p-5 flex flex-col bg-slate-950/40 text-white select-none box-border">
      {/* Status Bar */}
      <div class="flex items-center justify-between mb-4 border-b border-white/10 pb-2">
        <span class="text-xs font-black tracking-wider text-slate-400">HARDWARE CONTROLLER</span>
        <div class="flex items-center gap-1.5">
          <span class={`w-2 h-2 rounded-full ${isConnected() ? 'bg-green-500 animate-pulse' : 'bg-slate-500'}`} />
          <span class="text-[10px] font-mono uppercase text-slate-300">
            {isConnected() ? 'ESP8266 Connected' : 'Offline'}
          </span>
        </div>
      </div>

      {/* Main Grid */}
      <div class="flex flex-col gap-4 flex-1">
        {/* Color Preview & Readout */}
        <div class="flex items-center gap-4 bg-white/5 rounded-xl p-3 border border-white/10">
          <div 
            class="w-16 h-16 rounded-xl border border-white/20 transition-all duration-300 flex-shrink-0"
            style={{
              'background-color': `rgb(${r()}, ${g()}, ${b()})`,
              'box-shadow': `0 0 25px rgba(${r()}, ${g()}, ${b()}, 0.6)`
            }}
          />
          <div class="flex flex-col justify-center gap-0.5">
            <div class="text-[10px] font-mono text-slate-400 uppercase tracking-widest">Active RGB Value</div>
            <div class="text-lg font-black font-mono tracking-wider text-white">
              #{r().toString(16).padStart(2, '0').toUpperCase()}
              {g().toString(16).padStart(2, '0').toUpperCase()}
              {b().toString(16).padStart(2, '0').toUpperCase()}
            </div>
            <div class="text-[10px] font-mono text-slate-500">
              R:{r()} G:{g()} B:{b()}
            </div>
          </div>
        </div>

        {/* Sliders Container */}
        <div class="flex flex-col gap-3">
          {/* Red Slider */}
          <div class="flex flex-col gap-1">
            <div class="flex justify-between items-center text-[10px] font-mono">
              <span class="text-red-400 font-bold tracking-wider">RED CHANNEL</span>
              <span class="text-red-300 font-bold">{r()}</span>
            </div>
            <input 
              type="range" 
              min="0" 
              max="255" 
              value={r()} 
              onInput={(e) => handleSliderChange('r', e.target.value)}
              class="w-full h-1.5 rounded-lg appearance-none cursor-pointer bg-slate-800 accent-red-500"
              style={{
                background: `linear-gradient(to right, #000000, #ff0000)`
              }}
            />
          </div>

          {/* Green Slider */}
          <div class="flex flex-col gap-1">
            <div class="flex justify-between items-center text-[10px] font-mono">
              <span class="text-green-400 font-bold tracking-wider">GREEN CHANNEL</span>
              <span class="text-green-300 font-bold">{g()}</span>
            </div>
            <input 
              type="range" 
              min="0" 
              max="255" 
              value={g()} 
              onInput={(e) => handleSliderChange('g', e.target.value)}
              class="w-full h-1.5 rounded-lg appearance-none cursor-pointer bg-slate-800 accent-green-500"
              style={{
                background: `linear-gradient(to right, #000000, #00ff00)`
              }}
            />
          </div>

          {/* Blue Slider */}
          <div class="flex flex-col gap-1">
            <div class="flex justify-between items-center text-[10px] font-mono">
              <span class="text-blue-400 font-bold tracking-wider">BLUE CHANNEL</span>
              <span class="text-blue-300 font-bold">{b()}</span>
            </div>
            <input 
              type="range" 
              min="0" 
              max="255" 
              value={b()} 
              onInput={(e) => handleSliderChange('b', e.target.value)}
              class="w-full h-1.5 rounded-lg appearance-none cursor-pointer bg-slate-800 accent-blue-500"
              style={{
                background: `linear-gradient(to right, #000000, #0000ff)`
              }}
            />
          </div>
        </div>

        {/* Preset Presets */}
        <div class="flex flex-col gap-1.5 mt-2">
          <span class="text-[9px] font-mono tracking-widest text-slate-500 uppercase">Presets</span>
          <div class="grid grid-cols-4 gap-1.5">
            <For each={presets}>
              {(preset) => (
                <button
                  onClick={() => applyPreset(preset.r, preset.g, preset.b)}
                  class={`py-1 text-[10px] font-semibold rounded-md border text-center transition-all hover:scale-105 active:scale-95 ${preset.bgClass}`}
                >
                  {preset.name}
                </button>
              )}
            </For>
          </div>
        </div>
      </div>
    </div>
  );
};
