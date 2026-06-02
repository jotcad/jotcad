import { createSignal, onMount, onCleanup } from 'solid-js';
import { Cpu } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';

export const CounterWidget = (props) => {
  const [count, setCount] = createSignal(0);
  const [isPulsing, setIsPulse] = createSignal(false);
  const [isConnected, setIsConnected] = createSignal(true);
  const [pos, setPos] = createSignal({ x: props.x || 240, y: props.y || 40 });

  onMount(() => {
    let offlineTimer;
    const resetOfflineTimer = () => {
      if (offlineTimer) clearTimeout(offlineTimer);
      offlineTimer = setTimeout(() => {
        setIsConnected(false);
      }, 12000);
    };

    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        // Protocol Rule: Boundary Hydration ensures this is always a formal Selector
        if (selector.path === 'sensor/counter') {
            console.log('%c[CounterWidget] TICK:', 'color: #f59e0b; font-weight: bold;', payload.value);
            setCount(payload.value);
            setIsConnected(true);
            resetOfflineTimer();
            
            // Visual heartbeat
            setIsPulse(true);
            setTimeout(() => setIsPulse(false), 200);
        }
    };

    vfs.events.on('notify', handleNotify);

    // 1.5. Try to read initial state from VFS
    const initialSelector = new Selector('sensor/counter');
    vfsActions.readSelectorData(initialSelector)
      .then((data) => {
        if (data && typeof data.value !== 'undefined') {
          setCount(data.value);
          setIsConnected(true);
          resetOfflineTimer();
        }
      })
      .catch((err) => {
        console.warn('[CounterWidget] Initial sensor read failed (sensor disconnected):', err.message);
        setIsConnected(false);
      });

    // 2. Subscribe to the sensor
    // We do this every few minutes to keep the interest alive in the mesh
    const sub = () => {
        console.log('[CounterWidget] Renewing Mesh Subscription...');
        mesh.subscribe(new Selector('sensor/counter'), Date.now() + 1000 * 60 * 5);
    };
    
    sub();
    const subTimer = setInterval(sub, 1000 * 60 * 4);

    onCleanup(() => {
        vfs.events.off('notify', handleNotify);
        clearInterval(subTimer);
        if (offlineTimer) clearTimeout(offlineTimer);
    });
  });

  const handleMove = (newX, newY) => {
      setPos({ x: newX, y: newY });
      // Note: We don't persist this yet since it's an ephemeral widget,
      // but the UI will stay updated during the session.
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        label={isConnected() ? "ESP32 Sensor" : "Sensor (Offline)"}
        borderColor={isConnected() ? "border-amber-500/50" : "border-slate-500/30"}
        onMove={handleMove}
        class={isPulsing() ? 'scale-110 bg-amber-500/20 border-amber-400' : (!isConnected() ? 'opacity-60 bg-slate-800/10 border-slate-500/30' : '')}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-colors ${
                !isConnected() ? 'bg-slate-500 shadow-none' : (isPulsing() ? 'bg-amber-400 shadow-[0_0_8px_rgba(251,191,36,0.8)]' : 'bg-amber-900')
            }`} />
        }
    >
        {/* Counter Value on Icon Face */}
        <div class="absolute inset-0 flex items-center justify-center">
            <span class={`text-lg font-black font-mono transition-colors ${
                !isConnected() ? 'text-slate-500/60' : (isPulsing() ? 'text-amber-400' : 'text-white/40')
            }`}>
                {isConnected() ? count() : '?'}
            </span>
        </div>

        <div class="opacity-10">
            <Cpu size={32} />
        </div>
    </IconShell>
  );
};
