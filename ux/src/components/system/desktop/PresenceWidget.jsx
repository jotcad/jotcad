import { createSignal, onMount, onCleanup, Show, createMemo } from 'solid-js';
import { Eye, EyeOff, Activity } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { meshTopology } from '../../../lib/state/MeshState';

export const PresenceWidget = (props) => {
  const [state, setState] = createSignal(0); // 0: None, 1: Moving, 2: Stationary, 3: Both
  const [distance, setDistance] = createSignal(null);
  const [isPulsing, setIsPulse] = createSignal(false);
  const [lastUpdate, setLastUpdate] = createSignal(0);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 840 });

  // Check if any presence sensor is active in the mesh topology
  const isConnected = createMemo(() => {
    if (Date.now() - lastUpdate() < 15000) return true;
    return meshTopology.peers.some(p => 
      p.id.toLowerCase().includes('presence') || 
      p.id.toLowerCase().includes('radar') ||
      p.id.toLowerCase().includes('human')
    );
  });

  onMount(() => {
    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        if (selector.path === 'sensor/presence') {
            setState(typeof payload.state !== 'undefined' ? payload.state : 0);
            setDistance(typeof payload.distance_cm !== 'undefined' ? payload.distance_cm : null);
            setLastUpdate(Date.now());
            
            // Pulse whenever a packet comes
            setIsPulse(true);
            setTimeout(() => setIsPulse(false), 200);
        }
    };

    vfs.events.on('notify', handleNotify);

    // 2. Read initial state
    vfsActions.readSelectorData(new Selector('sensor/presence'))
      .then((data) => {
        if (data) {
          setState(typeof data.state !== 'undefined' ? data.state : 0);
          setDistance(typeof data.distance_cm !== 'undefined' ? data.distance_cm : null);
          setLastUpdate(Date.now());
        }
      })
      .catch(() => {});

    // 3. Subscribe to the sensor/presence path
    const sub = () => {
        console.log('[PresenceWidget] Renewing presence mesh subscription...');
        mesh.subscribe(new Selector('sensor/presence'), Date.now() + 1000 * 60 * 5).catch(() => {});
    };
    
    sub();
    const subTimer = setInterval(sub, 1000 * 60 * 4);

    onCleanup(() => {
        vfs.events.off('notify', handleNotify);
        clearInterval(subTimer);
    });
  });

  const handleMove = (newX, newY) => {
      setPos({ x: newX, y: newY });
  };

  const stateLabel = createMemo(() => {
      switch (state()) {
          case 1: return 'MOVE';
          case 2: return 'STILL';
          case 3: return 'BOTH';
          default: return 'CLEAR';
      }
  });

  // Color theme logic based on presence state
  const stateColorClass = createMemo(() => {
      if (!isConnected()) return 'text-slate-500/40';
      switch (state()) {
          case 1: return 'text-amber-400 drop-shadow-[0_0_6px_rgba(251,191,36,0.8)]'; // Moving
          case 2: return 'text-indigo-400 drop-shadow-[0_0_6px_rgba(129,140,248,0.8)]'; // Still
          case 3: return 'text-rose-400 drop-shadow-[0_0_6px_rgba(244,63,94,0.8)]'; // Both
          default: return 'text-emerald-500/50'; // Clear
      }
  });

  const getBorderColor = () => {
      if (!isConnected()) return 'border-slate-500/30';
      if (state() > 0) return 'border-indigo-500/50';
      return 'border-emerald-500/30';
  };

  const getBadgeColorClass = () => {
      if (!isConnected()) return 'bg-slate-500 shadow-none';
      if (isPulsing()) return 'bg-indigo-400 scale-125 shadow-[0_0_8px_rgba(129,140,248,0.8)]';
      if (state() > 0) return 'bg-indigo-600';
      return 'bg-emerald-950';
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        label={isConnected() ? "Presence" : "Radar (Offline)"}
        borderColor={getBorderColor()}
        onMove={handleMove}
        class={isPulsing() ? 'scale-110 bg-indigo-500/20 border-indigo-400' : (!isConnected() ? 'opacity-60 bg-slate-800/10 border-slate-500/30' : 'bg-indigo-950/10')}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-all ${getBadgeColorClass()}`} />
        }
    >
        {/* Presence Status Display */}
        <div class="absolute inset-0 flex flex-col items-center justify-center p-1 font-mono leading-none">
            <Show when={isConnected()} fallback={
                <EyeOff size={28} class="text-slate-500/40" />
            }>
                {/* Visual state text indicator */}
                <span class={`text-[10px] font-black tracking-widest ${stateColorClass()}`}>
                    {stateLabel()}
                </span>
                
                {/* Distance in cm */}
                <Show when={state() > 0 && distance() !== null}>
                    <span class="text-[9px] font-bold text-white/50 mt-1">
                        {distance()} cm
                    </span>
                </Show>
            </Show>
        </div>

        {/* Backdrop Icon for Radar sweep aesthetic */}
        <div class="opacity-[0.03] pointer-events-none absolute inset-0 flex items-center justify-center">
            <Activity size={36} />
        </div>
    </IconShell>
  );
};
