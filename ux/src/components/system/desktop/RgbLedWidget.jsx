import { createSignal, onMount, onCleanup, Show, createMemo } from 'solid-js';
import { Lightbulb, Sliders } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { windowActions } from '../../../lib/state/DesktopState';
import { meshTopology } from '../../../lib/state/MeshState';

export const RgbLedWidget = (props) => {
  const [r, setR] = createSignal(0);
  const [g, setG] = createSignal(0);
  const [b, setB] = createSignal(0);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 640 });
  const [isUpdating, setIsUpdating] = createSignal(false);

  // Check if RGB LED node is active in the mesh topology
  const isConnected = createMemo(() => {
    return meshTopology.peers.some(p => 
      p.id.toLowerCase().includes('rgb') || 
      p.id.toLowerCase().includes('led')
    );
  });

  onMount(() => {
    // 1. Listen to control state updates to keep widget visual in sync
    const handleNotify = (selector, payload) => {
      if (selector.path === 'control/rgb') {
        const red = typeof payload.r !== 'undefined' ? payload.r : (payload.red || 0);
        const green = typeof payload.g !== 'undefined' ? payload.g : (payload.green || 0);
        const blue = typeof payload.b !== 'undefined' ? payload.b : (payload.blue || 0);
        
        setR(red);
        setG(green);
        setB(blue);

        setIsUpdating(true);
        setTimeout(() => setIsUpdating(false), 200);
      }
    };

    vfs.events.on('notify', handleNotify);

    // 2. Initial state read from VFS cache
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
        // No initial state cached, default is 0, 0, 0
      });

    // 3. Keep interest alive in the Zenoh mesh
    const sub = () => {
      mesh.subscribe(rgbSel, Date.now() + 3600000).catch(() => {});
    };
    sub();
    const subTimer = setInterval(sub, 1800000); // Renew every 30 mins

    onCleanup(() => {
      vfs.events.off('notify', handleNotify);
      clearInterval(subTimer);
    });
  });

  const handleMove = (newX, newY) => {
    setPos({ x: newX, y: newY });
  };

  const handleWidgetClick = () => {
    windowActions.open('rgb_led', 'rgb_led_win', {
      label: 'RGB LED Control',
      size: { width: 330, height: 420 }
    });
  };

  // Compute glowing color style
  const glowStyle = createMemo(() => {
    const isOff = r() === 0 && g() === 0 && b() === 0;
    if (!isConnected() || isOff) return 'text-slate-500/40';
    return `drop-shadow(0 0 8px rgba(${r()}, ${g()}, ${b()}, 0.85))`;
  });

  const rgbColorString = createMemo(() => {
    return `rgb(${r()}, ${g()}, ${b()})`;
  });

  return (
    <IconShell
      x={pos().x}
      y={pos().y}
      onClick={handleWidgetClick}
      label={isConnected() ? "RGB LED" : "RGB LED (Offline)"}
      borderColor={isConnected() ? "border-violet-500/50" : "border-slate-500/30"}
      onMove={handleMove}
      class={isConnected() ? 'bg-violet-950/10' : 'opacity-60'}
      badge={
        <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-all ${
          !isConnected() ? 'bg-slate-500 shadow-none' : (isUpdating() ? 'bg-violet-400 scale-125' : 'bg-violet-800')
        }`} />
      }
    >
      <div class="absolute inset-0 flex items-center justify-center overflow-hidden rounded-xl">
        <Lightbulb 
          size={32} 
          style={isConnected() && (r() > 0 || g() > 0 || b() > 0) ? { filter: glowStyle(), color: rgbColorString() } : {}}
          class={`transition-all duration-300 ${isConnected() && (r() > 0 || g() > 0 || b() > 0) ? '' : 'text-slate-500/40'}`}
        />
      </div>

      {/* Embedded sliders hint */}
      <div class="absolute bottom-1 right-2 opacity-30 group-hover:opacity-75 transition-opacity">
        <Sliders size={10} class="text-white" />
      </div>
    </IconShell>
  );
};
