import { createSignal, onMount, onCleanup, Show } from 'solid-js';
import { Camera, RefreshCw } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';

export const CameraWidget = (props) => {
  const [image, setImage] = createSignal(null);
  const [lastUpdate, setLastUpdate] = createSignal(0);
  const [isConnected, setIsConnected] = createSignal(false);
  const [isUpdating, setIsUpdating] = createSignal(false);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 40 });

  onMount(() => {
    let offlineTimer;
    const resetOfflineTimer = () => {
      if (offlineTimer) clearTimeout(offlineTimer);
      offlineTimer = setTimeout(() => {
        setIsConnected(false);
      }, 15000); // Camera is slower, 15s timeout
    };

    const handleNotify = (selector, payload) => {
        if (selector.path === 'sensor/camera') {
            setImage(payload.image);
            setLastUpdate(Date.now());
            setIsConnected(true);
            setIsUpdating(true);
            setTimeout(() => setIsUpdating(false), 500);
            resetOfflineTimer();
        }
    };

    vfs.events.on('notify', handleNotify);

    // Subscribe to the camera feed
    const sub = () => {
        mesh.subscribe(new Selector('sensor/camera'), Date.now() + 60000).catch(() => {});
    };
    sub();
    const subTimer = setInterval(sub, 45000);

    // Initial read attempt
    vfsActions.readData(new Selector('sensor/camera'))
        .then(data => {
            if (data && data.image) {
                setImage(data.image);
                setIsConnected(true);
                resetOfflineTimer();
            }
        })
        .catch(() => {
            setIsConnected(false);
        });

    onCleanup(() => {
        vfs.events.off('notify', handleNotify);
        clearInterval(subTimer);
        if (offlineTimer) clearTimeout(offlineTimer);
    });
  });

  const handleMove = (dx, dy) => {
    setPos(p => ({ x: p.x + dx, y: p.y + dy }));
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        label={isConnected() ? "ESP32-CAM" : "Camera (Offline)"}
        borderColor={isConnected() ? "border-cyan-500/50" : "border-slate-500/30"}
        onMove={handleMove}
        class={isConnected() ? 'bg-cyan-500/5' : 'opacity-60'}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-all ${
                !isConnected() ? 'bg-slate-500' : (isUpdating() ? 'bg-cyan-400 scale-125 shadow-[0_0_8px_rgba(34,211,238,0.8)]' : 'bg-cyan-900')
            }`} />
        }
    >
        <div class="absolute inset-0 flex items-center justify-center overflow-hidden rounded-xl">
            <Show when={image() && isConnected()} fallback={
                <Camera size={32} class={isConnected() ? 'text-cyan-500/40 animate-pulse' : 'text-slate-500/40'} />
            }>
                <img 
                    src={image()} 
                    class="w-full h-full object-cover opacity-80 group-hover:opacity-100 transition-opacity" 
                />
            </Show>
        </div>

        {/* Small metadata overlay if connected */}
        <Show when={isConnected()}>
            <div class="absolute bottom-1 right-2 pointer-events-none">
                <span class="text-[8px] font-mono text-cyan-500/60 uppercase">Live</span>
            </div>
        </Show>
    </IconShell>
  );
};
