import { createSignal, onMount, onCleanup, Show, createMemo } from 'solid-js';
import { Music, Disc } from 'lucide-solid';
import { IconShell } from './IconShell';
import { vfs, mesh, Selector, vfsActions } from '../../../lib/VFSManager';
import { windowActions } from '../../../lib/state/DesktopState';
import { meshTopology } from '../../../lib/state/MeshState';

export const SynthWidget = (props) => {
  const [lastNote, setLastNote] = createSignal('');
  const [velocity, setVelocity] = createSignal(0);
  const [eventType, setEventType] = createSignal(null); // 'note_on' | 'note_off' | 'control_change'
  const [isPulsing, setIsPulse] = createSignal(false);
  const [lastUpdate, setLastUpdate] = createSignal(0);
  const [pos, setPos] = createSignal({ x: props.x || 40, y: props.y || 940 });

  // Map MIDI note numbers to note names
  const midiToNoteName = (noteNum) => {
    const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
    const octave = Math.floor(noteNum / 12) - 1;
    const noteIndex = noteNum % 12;
    return `${noteNames[noteIndex]}${octave}`;
  };

  // Check if any synth or MIDI reader node is connected in the mesh topology
  const isConnected = createMemo(() => {
    if (Date.now() - lastUpdate() < 15000) return true;
    return meshTopology.peers.some(p => 
      p.id.toLowerCase().includes('synth') || 
      p.id.toLowerCase().includes('midi') ||
      p.id.toLowerCase().includes('music')
    );
  });

  onMount(() => {
    // 1. Hook into VFS events
    const handleNotify = (selector, payload) => {
        if (selector.path === 'midi/event' || selector.path === 'midi/events') {
            setLastUpdate(Date.now());
            const type = payload.type || '';
            setEventType(type);
            
            if (type === 'note_on') {
                const noteName = midiToNoteName(payload.note || 60);
                setLastNote(noteName);
                setVelocity(payload.velocity || 100);
                
                // Trigger flash animation
                setIsPulse(true);
                setTimeout(() => setIsPulse(false), 150);
            } else if (type === 'note_off') {
                // Keep the last note visible but dim velocity or state
                setVelocity(0);
            } else if (type === 'control_change') {
                setLastNote(`CC${payload.control}`);
                setVelocity(payload.value || 0);
                setIsPulse(true);
                setTimeout(() => setIsPulse(false), 150);
            }
        }
    };

    vfs.events.on('notify', handleNotify);

    // 2. Initial state read from VFS
    vfsActions.readSelectorData(new Selector('midi/event'))
      .then((data) => {
        if (data) {
          setLastUpdate(Date.now());
          const type = data.type || '';
          setEventType(type);
          if (type === 'note_on' || type === 'control_change') {
              setLastNote(type === 'note_on' ? midiToNoteName(data.note || 60) : `CC${data.control}`);
              setVelocity(data.velocity || data.value || 0);
          }
        }
      })
      .catch(() => {});

    // 3. Subscribe to the MIDI events
    const sub = () => {
        console.log('[SynthWidget] Renewing MIDI mesh subscriptions...');
        mesh.subscribe(new Selector('midi/event'), Date.now() + 1000 * 60 * 5).catch(() => {});
        mesh.subscribe(new Selector('midi/events'), Date.now() + 1000 * 60 * 5).catch(() => {});
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

  const handleWidgetClick = () => {
      windowActions.open('instruments', 'instruments_win', {
          label: 'Synthesizer Panel',
          size: { width: 620, height: 500 }
      });
  };

  // Determine display styles based on note velocity / event type
  const noteColorClass = createMemo(() => {
      if (!isConnected()) return 'text-slate-500/40';
      if (eventType() === 'control_change') return 'text-cyan-400 drop-shadow-[0_0_6px_rgba(34,211,238,0.8)]';
      if (velocity() > 0) return 'text-pink-400 drop-shadow-[0_0_6px_rgba(244,63,94,0.8)]';
      return 'text-pink-500/40';
  });

  return (
    <IconShell
        x={pos().x}
        y={pos().y}
        onClick={handleWidgetClick}
        label={isConnected() ? "Synth Node" : "Synth (Offline)"}
        borderColor={isConnected() ? "border-pink-500/50" : "border-slate-500/30"}
        onMove={handleMove}
        class={isPulsing() ? 'scale-115 bg-pink-500/20 border-pink-400' : (!isConnected() ? 'opacity-60 bg-slate-800/10 border-slate-500/30' : 'bg-pink-950/10')}
        badge={
            <div class={`absolute -top-1 -right-1 w-3 h-3 rounded-full border-2 border-black transition-all ${
                !isConnected() ? 'bg-slate-500 shadow-none' : (isPulsing() ? 'bg-pink-400 scale-125 shadow-[0_0_8px_rgba(244,63,94,0.8)]' : 'bg-pink-800')
            }`} />
        }
    >
        {/* Synth Event display */}
        <div class="absolute inset-0 flex flex-col items-center justify-center p-1 font-mono leading-none">
            <Show when={isConnected() && lastNote()} fallback={
                <Music size={28} class="text-slate-500/40" />
            }>
                {/* Visual note / CC index */}
                <span class={`text-xs font-black tracking-wider ${noteColorClass()}`}>
                    {lastNote()}
                </span>
                
                {/* Velocity / CC value indicator */}
                <Show when={velocity() > 0}>
                    <span class="text-[8px] text-white/50 mt-1 uppercase tracking-widest">
                        {eventType() === 'control_change' ? `Val:${velocity()}` : `Vel:${velocity()}`}
                    </span>
                </Show>
            </Show>
        </div>

        {/* Ambient background music icon */}
        <div class="opacity-[0.03] pointer-events-none absolute inset-0 flex items-center justify-center">
            <Disc size={36} class={velocity() > 0 ? 'animate-spin' : ''} />
        </div>
    </IconShell>
  );
};
