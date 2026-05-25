import { createSignal, onMount, onCleanup } from 'solid-js';
import { Cpu } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector } from '../../../lib/VFSManager';

export const CounterWidget = (props) => {
  const [count, setCount] = createSignal(0);
  const [isPulsing, setIsPulse] = createSignal(false);
  const [pos, setPos] = createSignal({ x: props.x || 240, y: props.y || 40 });

  onMount(() => {
    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        // Protocol Rule: Boundary Hydration ensures this is always a formal Selector
        if (selector.path === 'sensor/counter') {
            console.log('%c[CounterWidget] TICK:', 'color: #f59e0b; font-weight: bold;', payload.value);
            setCount(payload.value);
            
            // Visual heartbeat
            setIsPulse(true);
            setTimeout(() => setIsPulse(false), 200);
        }
    };

    vfs.events.on('notify', handleNotify);

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
        label="ESP32 Sensor"
        borderColor="border-amber-500/50"
        onMove={handleMove}
        class={isPulsing() ? 'scale-110 bg-amber-500/20 border-amber-400' : ''}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-colors ${
                isPulsing() ? 'bg-amber-400 shadow-[0_0_8px_rgba(251,191,36,0.8)]' : 'bg-amber-900'
            }`} />
        }
    >
        {/* Counter Value on Icon Face */}
        <div class="absolute inset-0 flex items-center justify-center">
            <span class={`text-lg font-black font-mono transition-colors ${isPulsing() ? 'text-amber-400' : 'text-white/40'}`}>
                {count()}
            </span>
        </div>

        <div class="opacity-10">
            <Cpu size={32} />
        </div>
    </IconShell>
  );
};
