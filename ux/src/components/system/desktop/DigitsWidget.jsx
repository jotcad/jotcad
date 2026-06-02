import { createSignal, onMount, onCleanup, Show } from 'solid-js';
import { Binary, Eye } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';

export const DigitsWidget = (props) => {
  const [digits, setDigits] = createSignal('');
  const [confidence, setConfidence] = createSignal(1.0);
  const [isPulsing, setIsPulse] = createSignal(false);
  const [isConnected, setIsConnected] = createSignal(false);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 540 });

  onMount(() => {
    let offlineTimer;
    const resetOfflineTimer = () => {
      if (offlineTimer) clearTimeout(offlineTimer);
      offlineTimer = setTimeout(() => {
        setIsConnected(false);
      }, 10000); // 10-second timeout
    };

    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        // Protocol Rule: Boundary Hydration ensures selector is a formal instance
        if (selector.path === 'sensor/vision/digits') {
            console.log('%c[DigitsWidget] OCR TICK:', 'color: #10b981; font-weight: bold;', payload.digits, `(${Math.round(payload.confidence * 100)}%)`);
            setDigits(payload.digits);
            setConfidence(payload.confidence);
            setIsConnected(true);
            resetOfflineTimer();
            
            // Visual heartbeat pulse
            setIsPulse(true);
            setTimeout(() => setIsPulse(false), 300);
        }
    };

    vfs.events.on('notify', handleNotify);

    // 1.5. Try to read initial state from VFS
    const initialSelector = new Selector('sensor/vision/digits');
    vfsActions.readData(initialSelector)
      .then((data) => {
        if (data && typeof data.digits !== 'undefined') {
          setDigits(data.digits);
          setConfidence(data.confidence || 1.0);
          setIsConnected(true);
          resetOfflineTimer();
        }
      })
      .catch((err) => {
        console.warn('[DigitsWidget] Initial vision read failed:', err.message);
        setIsConnected(false);
      });

    // 2. Subscribe to the OCR telemetry topic
    const sub = () => {
        // Use a 1-hour TTL (3,600,000ms) to maximize stability
        mesh.subscribe(new Selector('sensor/vision/digits'), Date.now() + 3600000).catch(() => {});
    };
    
    sub();
    const subTimer = setInterval(sub, 1800000); // Renew every 30 minutes

    onCleanup(() => {
        vfs.events.off('notify', handleNotify);
        clearInterval(subTimer);
        if (offlineTimer) clearTimeout(offlineTimer);
    });
  });

  const handleMove = (newX, newY) => {
      setPos({ x: newX, y: newY });
  };

  // Determine border color based on confidence & connection state
  const getBorderColor = () => {
      if (!isConnected()) return 'border-slate-500/30';
      if (confidence() > 0.85) return 'border-emerald-500/50';
      if (confidence() > 0.60) return 'border-amber-500/50';
      return 'border-rose-500/50';
  };

  const getBadgeColorClass = () => {
      if (!isConnected()) return 'bg-slate-500 shadow-none';
      if (isPulsing()) return 'bg-emerald-400 scale-125 shadow-[0_0_8px_rgba(16,185,129,0.8)]';
      if (confidence() > 0.85) return 'bg-emerald-600';
      if (confidence() > 0.60) return 'bg-amber-600';
      return 'bg-rose-600';
  };

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        label={isConnected() ? "Digits OCR" : "OCR (Offline)"}
        borderColor={getBorderColor()}
        onMove={handleMove}
        class={isPulsing() ? 'scale-110 bg-emerald-500/20 border-emerald-400' : (!isConnected() ? 'opacity-60 bg-slate-800/10 border-slate-500/30' : 'bg-emerald-950/10')}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-all ${getBadgeColorClass()}`} />
        }
    >
        {/* Dynamic Glowing Value Display */}
        <div class="absolute inset-0 flex flex-col items-center justify-center p-1 overflow-hidden rounded-xl">
            <Show when={isConnected()} fallback={
                <Binary size={28} class="text-slate-500/40" />
            }>
                {/* Digit Display with glowing Nixie/LED effect */}
                <span class={`text-lg font-black font-mono tracking-widest transition-all duration-300 ${
                    confidence() > 0.85 
                      ? 'text-emerald-400 drop-shadow-[0_0_6px_rgba(52,211,153,0.8)]' 
                      : (confidence() > 0.60 ? 'text-amber-400 drop-shadow-[0_0_6px_rgba(251,191,36,0.8)]' : 'text-rose-400 drop-shadow-[0_0_6px_rgba(244,63,94,0.8)]')
                }`}>
                    {digits() !== '' ? digits() : '—'}
                </span>
                
                {/* Micro mini confidence label */}
                <span class="text-[8px] font-mono text-white/30 uppercase mt-0.5 tracking-tighter">
                    {Math.round(confidence() * 100)}% acc
                </span>
            </Show>
        </div>

        {/* Backdrop icon decorative element */}
        <div class="opacity-[0.04] pointer-events-none absolute inset-0 flex items-center justify-center">
            <Eye size={36} />
        </div>
    </IconShell>
  );
};
