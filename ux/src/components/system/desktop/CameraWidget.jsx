import { createSignal, onMount, onCleanup, Show } from 'solid-js';
import { Camera, RefreshCw } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { windowActions } from '../../../lib/state/DesktopState';

export const CameraWidget = (props) => {
  const [image, setImage] = createSignal(null);
  const [lastUpdate, setLastUpdate] = createSignal(0);
  const [isConnected, setIsConnected] = createSignal(false);
  const [isUpdating, setIsUpdating] = createSignal(false);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 40 });

    const decodeCameraPayload = (selector, payload) => {
        if (!payload) return null;
        if (payload instanceof Uint8Array) {
            // Check if format is grayscale, default to raw grayscale (160x120 = 19200 bytes) if not specified
            const format = selector?.parameters?.format || (payload.length === 19200 ? 'grayscale' : 'jpeg');
            if (format === 'grayscale') {
                const width = selector?.parameters?.width || 160;
                const height = selector?.parameters?.height || 120;
                
                const canvas = document.createElement('canvas');
                canvas.width = width;
                canvas.height = height;
                const ctx = canvas.getContext('2d');
                const imageData = ctx.createImageData(width, height);
                for (let i = 0; i < payload.length; i++) {
                    const val = payload[i];
                    const j = i * 4;
                    imageData.data[j] = val;     // R
                    imageData.data[j + 1] = val; // G
                    imageData.data[j + 2] = val; // B
                    imageData.data[j + 3] = 255; // A
                }
                ctx.putImageData(imageData, 0, 0);
                return canvas.toDataURL('image/png');
            } else {
                const blob = new Blob([payload], { type: 'image/jpeg' });
                return URL.createObjectURL(blob);
            }
        } else if (payload.format === 'grayscale') {
            // Fallback for json format
            const width = payload.width || 96;
            const height = payload.height || 96;
            let base64Str = payload.image;
            if (base64Str.includes(',')) base64Str = base64Str.split(',')[1];
            const binaryString = atob(base64Str);
            const bytes = new Uint8Array(binaryString.length);
            for (let i = 0; i < binaryString.length; i++) {
                bytes[i] = binaryString.charCodeAt(i);
            }
            const canvas = document.createElement('canvas');
            canvas.width = width;
            canvas.height = height;
            const ctx = canvas.getContext('2d');
            const imageData = ctx.createImageData(width, height);
            for (let i = 0; i < bytes.length; i++) {
                const val = bytes[i];
                const j = i * 4;
                imageData.data[j] = val;
                imageData.data[j + 1] = val;
                imageData.data[j + 2] = val;
                imageData.data[j + 3] = 255;
            }
            ctx.putImageData(imageData, 0, 0);
            return canvas.toDataURL('image/png');
        } else {
            return payload.image || null;
        }
    };

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
                const url = decodeCameraPayload(selector, payload);
                if (!url) return;
    
                // Revoke previous URL to prevent memory leaks
                const prev = image();
                if (prev && prev.startsWith('blob:')) {
                    URL.revokeObjectURL(prev);
                }
    
                setImage(url);
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
            // Protocol Rule: Match the exact parameters produced by the OCR node (96x96 grayscale)
            const camSel = new Selector('sensor/camera', { width: 96, height: 96, format: 'grayscale' });
            mesh.subscribe(camSel, Date.now() + 3600000).catch(() => {});
        };
        sub();
        // Heartbeat at 30 minutes to maintain a safe overlap
        const subTimer = setInterval(sub, 1800000);
    
        // Initial read attempt
        const camSel = new Selector('sensor/camera', { width: 96, height: 96, format: 'grayscale' });
        vfsActions.readSelectorData(camSel)
            .then(data => {
                const url = decodeCameraPayload(camSel, data);
                if (url) {
                    setImage(url);
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

  const handleWidgetClick = () => {
    windowActions.open('vision_debug', 'vision_debug_win', {
      label: 'Camera OCR Diagnostics',
      size: { width: 500, height: 320 }
    });
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        onClick={handleWidgetClick}
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
