import { createSignal, onMount, onCleanup, Show, createMemo } from 'solid-js';
import { Thermometer, Droplets, Wind } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { meshTopology } from '../../../lib/state/MeshState';

export const ClimateWidget = (props) => {
  const [temp, setTemp] = createSignal(null);
  const [humidity, setHumidity] = createSignal(null);
  const [voc, setVoc] = createSignal(null);
  const [isPulsing, setIsPulse] = createSignal(false);
  const [lastUpdate, setLastUpdate] = createSignal(0);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 740 });

  // Check if any climate/environment or VOC sensor is present in topology
  const isConnected = createMemo(() => {
    // If we've received data recently (in the last 15 seconds), consider it connected
    if (Date.now() - lastUpdate() < 15000) return true;
    return meshTopology.peers.some(p => 
      p.id.toLowerCase().includes('dht11') || 
      p.id.toLowerCase().includes('sgp40') || 
      p.id.toLowerCase().includes('env') || 
      p.id.toLowerCase().includes('voc')
    );
  });

  onMount(() => {
    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        let changed = false;
        
        if (selector.path === 'sensor/environment') {
            if (typeof payload.temperature !== 'undefined') {
                setTemp(payload.temperature);
                changed = true;
            }
            if (typeof payload.humidity !== 'undefined') {
                setHumidity(payload.humidity);
                changed = true;
            }
        } else if (selector.path === 'sensor/temperature') {
            if (typeof payload.value !== 'undefined') {
                setTemp(payload.value);
                changed = true;
            }
        } else if (selector.path === 'sensor/humidity') {
            if (typeof payload.value !== 'undefined') {
                setHumidity(payload.value);
                changed = true;
            }
        } else if (selector.path === 'sensor/voc') {
            if (typeof payload.value !== 'undefined') {
                setVoc(payload.value);
                changed = true;
            }
        }

        if (changed) {
            setLastUpdate(Date.now());
            setIsPulse(true);
            setTimeout(() => setIsPulse(false), 200);
        }
    };

    vfs.events.on('notify', handleNotify);

    // 2. Read initial values from VFS caches (using explicit Selector path)
    const readInitialData = async () => {
        try {
            const envData = await vfsActions.readSelectorData(new Selector('sensor/environment'));
            if (envData) {
                if (typeof envData.temperature !== 'undefined') setTemp(envData.temperature);
                if (typeof envData.humidity !== 'undefined') setHumidity(envData.humidity);
                setLastUpdate(Date.now());
            }
        } catch (_) {}

        try {
            const tempData = await vfsActions.readSelectorData(new Selector('sensor/temperature'));
            if (tempData && typeof tempData.value !== 'undefined') {
                setTemp(tempData.value);
                setLastUpdate(Date.now());
            }
        } catch (_) {}

        try {
            const humData = await vfsActions.readSelectorData(new Selector('sensor/humidity'));
            if (humData && typeof humData.value !== 'undefined') {
                setHumidity(humData.value);
                setLastUpdate(Date.now());
            }
        } catch (_) {}

        try {
            const vocData = await vfsActions.readSelectorData(new Selector('sensor/voc'));
            if (vocData && typeof vocData.value !== 'undefined') {
                setVoc(vocData.value);
                setLastUpdate(Date.now());
            }
        } catch (_) {}
    };

    readInitialData();

    // 3. Renew subscriptions to the mesh environmental topics periodically
    const sub = () => {
        console.log('[ClimateWidget] Renewing environment mesh subscriptions...');
        const ttl = Date.now() + 1000 * 60 * 5; // 5 min TTL
        mesh.subscribe(new Selector('sensor/environment'), ttl).catch(() => {});
        mesh.subscribe(new Selector('sensor/temperature'), ttl).catch(() => {});
        mesh.subscribe(new Selector('sensor/humidity'), ttl).catch(() => {});
        mesh.subscribe(new Selector('sensor/voc'), ttl).catch(() => {});
    };
    
    sub();
    const subTimer = setInterval(sub, 1000 * 60 * 4); // every 4 mins

    onCleanup(() => {
        vfs.events.off('notify', handleNotify);
        clearInterval(subTimer);
    });
  });

  const handleMove = (newX, newY) => {
      setPos({ x: newX, y: newY });
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        label={isConnected() ? "Climate" : "Climate (Offline)"}
        borderColor={isConnected() ? "border-cyan-500/50" : "border-slate-500/30"}
        onMove={handleMove}
        class={isPulsing() ? 'scale-110 bg-cyan-500/20 border-cyan-400' : (!isConnected() ? 'opacity-60 bg-slate-800/10 border-slate-500/30' : 'bg-cyan-950/10')}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-colors ${
                !isConnected() ? 'bg-slate-500 shadow-none' : (isPulsing() ? 'bg-cyan-400 shadow-[0_0_8px_rgba(34,211,238,0.8)]' : 'bg-cyan-800')
            }`} />
        }
    >
        {/* Climate Info Display */}
        <div class="absolute inset-0 flex flex-col items-center justify-center p-1 font-mono leading-tight">
            <Show when={isConnected() && (temp() !== null || humidity() !== null)} fallback={
                <Thermometer size={28} class="text-slate-500/40" />
            }>
                <div class="text-[11px] font-black text-cyan-400 drop-shadow-[0_0_4px_rgba(34,211,238,0.6)]">
                    {temp() !== null ? `${temp().toFixed(1)}°` : '—'}
                </div>
                <div class="text-[9px] font-bold text-white/50 flex items-center gap-0.5 mt-0.5">
                    <Droplets size={8} class="text-sky-400" />
                    {humidity() !== null ? `${humidity().toFixed(0)}%` : '—'}
                </div>
                <Show when={voc() !== null}>
                    <div class="text-[7px] font-extrabold text-emerald-400 flex items-center gap-0.5 tracking-tighter mt-0.5">
                        <Wind size={6} />
                        VOC:{voc()}
                    </div>
                </Show>
            </Show>
        </div>

        {/* Ambient Icon Backdrop */}
        <div class="opacity-[0.03] pointer-events-none absolute inset-0 flex items-center justify-center">
            <Thermometer size={36} />
        </div>
    </IconShell>
  );
};
