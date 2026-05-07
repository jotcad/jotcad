import { createSignal, onMount, onCleanup } from 'solid-js';
import interact from 'interactjs';
import { vfs } from '../../lib/blackboard';
import { Selector } from '../../../fs/src/cid';

const DEFAULT_CODE = `
// Define your agent here using standard VFS registration
// Note: Selector is injected globally for you to use.

export const agent = {
  register(vfs) {
    // Register as the provider for this specific UI output path
    this.unsubscribe = vfs.registerProvider('ui/result/hexagon', async (v, selector) => {
      console.log('[HexagonPipeline] Waking up to fulfill result...');
      
      // Request the required geometry from the mesh
      const hexagon = await vfs.readData(new Selector('jot/Hexagon/full', { diameter: 50 }).withOutput('$out'));
      console.log('[HexagonPipeline] Received C++ geometry:', hexagon);
      
      // Return the final result to whoever requested ui/result/hexagon
      return {
          ...hexagon,
          processedBy: 'Jot Engine',
          timestamp: new Date().toISOString()
      };
    });
  },
  
  stop() {
    if (this.unsubscribe) this.unsubscribe();
  }
};
`;

// Exported component
export const ScriptNode = (props) => {
  let nodeRef;
  const [pos, setPos] = createSignal({ x: 400, y: 100 });
  const [code, setCode] = createSignal(DEFAULT_CODE);
  const [isRunning, setIsRunning] = createSignal(false);
  let activeAgent = null;

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          setPos({ x: pos().x + event.dx, y: pos().y + event.dy });
        },
      },
    });
  });

  const toggleAgent = async () => {
    if (isRunning()) {
      activeAgent?.stop();
      activeAgent = null;
      setIsRunning(false);
    } else {
      try {
        // Inject Selector into global scope so the script can use it
        globalThis.Selector = Selector;

        // Evaluate the code
        // Note: For a real app, use a safer sandbox or a proper module loader.
        const blob = new Blob([code()], { type: 'application/javascript' });
        const url = URL.createObjectURL(blob);
        const module = await import(/* @vite-ignore */ url);
        URL.revokeObjectURL(url);

        if (!module.agent)
          throw new Error('Exported "agent" object not found.');

        // Instantiate and start the agent
        activeAgent = module.agent;
        activeAgent.register(vfs);

        setIsRunning(true);
      } catch (err) {
        console.error('[ScriptNode] Failed to start agent:', err);
        alert('Failed to start agent: ' + err.message);
      }
    }
  };

  onCleanup(() => {
    activeAgent?.stop();
  });

  return (
    <div
      ref={nodeRef}
      class="absolute select-none p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden flex flex-col gap-3"
      style={{
        transform: `translate(${pos().x}px, ${pos().y}px)`,
        width: '400px',
        'z-index': 20,
      }}
    >
      <div class="flex justify-between items-center">
        <div class="text-xs font-black uppercase tracking-tighter text-white/40">
          Jot Engine
        </div>
        <div
          class={`w-2 h-2 rounded-full ${
            isRunning() ? 'bg-available animate-pulse' : 'bg-white/10'
          }`}
        ></div>
      </div>

      <textarea
        class="bg-black/40 border border-white/5 rounded-lg p-3 font-mono text-[11px] h-64 focus:outline-none focus:border-provisioning text-available resize-none"
        value={code()}
        onInput={(e) => setCode(e.target.value)}
        spellcheck={false}
      />

      <div class="flex gap-2">
        <button
          onClick={toggleAgent}
          class={`flex-1 py-2 rounded-lg text-sm font-bold transition-all ${
            isRunning()
              ? 'bg-red-500/20 text-red-400 hover:bg-red-500/30'
              : 'bg-provisioning text-black hover:bg-provisioning/80'
          }`}
        >
          {isRunning() ? 'STOP AGENT' : 'RUN AGENT'}
        </button>
      </div>
    </div>
  );
};
