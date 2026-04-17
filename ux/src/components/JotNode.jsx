import { createSignal, onMount, For, Show } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard } from '../lib/blackboard';
import { Database, Minus, Maximize2, Layers } from 'lucide-solid';
import { JotParser } from '../../../jot/src/parser';
import { JotCompiler } from '../../../jot/src/compiler';
import { Viewport } from './Viewport';
import { packZFS } from '../lib/three_utils';

const DEFAULT_CODE = `
// JotCAD Expressions
Hexagon(30)
`.trim();

export const JotNode = (props) => {
  let nodeRef;
  const [pos, setPos] = createSignal({ x: 400, y: 400 });
  const [code, setCode] = createSignal(DEFAULT_CODE);
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(false);
  const [resultData, setResultData] = createSignal(null);

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          setPos({ x: pos().x + event.dx, y: pos().y + event.dy });
        },
      },
    });

    // Auto-evaluate on mount
    evaluateJot();
  });

  const evaluateJot = async () => {
    if (isEvaluating()) return;
    setIsEvaluating(true);
    try {
      console.log('[JotNode] Compiling expression...');
      const parser = new JotParser();
      const compiler = new JotCompiler(vfs);

      // Register all discovered schemas with the compiler
      const currentSchemas = blackboard.schemas();
      for (const [path, schema] of Object.entries(currentSchemas)) {
        // 1. Canonical Name (e.g. jot/Box -> Box)
        if (path.startsWith('jot/')) {
          compiler.registerOperator(path.slice(4), { path, schema });
        }
        
        // 2. Metadata Alias (e.g. jot/Hexagon -> Hexagon)
        const alias = schema.metadata?.alias;
        if (alias && alias.startsWith('jot/')) {
          compiler.registerOperator(alias.slice(4), { path, schema });
        }
      }

      const ast = parser.parse(code());
      const resolved = await compiler.evaluate(ast);

      console.log(
        '[JotNode] Translated to selector:',
        JSON.stringify(resolved, null, 2)
      );

      // Handle Sequences (Arrays) - if array, wrap in Group for visualization
      let finalSelector = resolved;
      if (Array.isArray(resolved)) {
        finalSelector = { path: 'jot/group', parameters: { $in: resolved } };
      }

      const data = await vfs.readData(finalSelector.path, finalSelector.parameters);
      
      if (data) {
        console.log('[JotNode] RAW SHAPE DATA:', JSON.stringify(data, null, 2));
        
        // Pack into self-contained ZFS for robust visualization and debugging
        if (typeof data === 'object' && (data.geometry || data.shapes)) {
          const unified = await packZFS(vfs, data);
          setResultData(unified);
        } else {
          setResultData(data);
        }
        
        console.log('[JotNode] Visualization updated locally.');
      } else {
        console.warn(`[JotNode] Mesh resolution failed: No data for ${finalSelector.path}`);
      }
    } catch (err) {
      console.error('[JotNode] Compilation/Evaluation failed:', err);
      alert('Error: ' + err.message);
    } finally {
      setIsEvaluating(false);
    }
  };

  return (
    <div
      ref={nodeRef}
      class="absolute select-none p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden flex flex-col gap-3"
      style={{
        left: `${pos().x}px`,
        top: `${pos().y}px`,
        width: isMinimized() ? '200px' : '500px',
        'z-index': 20,
        transition: 'width 0.2s, height 0.2s',
      }}
    >
      <div
        class="flex justify-between items-center cursor-move"
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="text-xs font-black uppercase tracking-tighter text-white/40">
          Jot Engine Expression
        </div>
        <div class="flex items-center gap-2">
          <div
            class={`w-2 h-2 rounded-full ${
              isEvaluating() ? 'bg-cyan-400 animate-pulse' : 'bg-white/10'
            }`}
          ></div>
          <button
            class="text-white/20 hover:text-white transition-colors"
            onClick={() => setIsMinimized(!isMinimized())}
          >
            {isMinimized() ? <Maximize2 size={12} /> : <Minus size={12} />}
          </button>
        </div>
      </div>

      <Show when={!isMinimized()}>
        <textarea
          class="bg-black/40 border border-white/5 rounded-lg p-3 font-mono text-[11px] h-32 focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar"
          value={code()}
          onInput={(e) => setCode(e.target.value)}
          onKeyDown={(e) => {
            if (e.shiftKey && e.key === 'Enter') {
              e.preventDefault();
              evaluateJot();
            }
          }}
          spellcheck={false}
        />

        <div class="flex gap-2">
          <button
            onClick={evaluateJot}
            disabled={isEvaluating()}
            class={`flex-1 py-2 rounded-lg text-sm font-bold transition-all ${
              isEvaluating()
                ? 'bg-cyan-500/20 text-cyan-400'
                : 'bg-cyan-400 text-black hover:bg-cyan-300'
            }`}
          >
            {isEvaluating() ? 'EVALUATING...' : 'EVALUATE JOT'}
          </button>
        </div>

        {/* Local Visualization Viewport */}
        <div class="w-full h-64 bg-black/40 rounded-lg border border-white/10 overflow-hidden relative group mt-1">
          <Show 
            when={resultData()} 
            fallback={
              <div class="w-full h-full flex flex-col items-center justify-center gap-2 opacity-20 transition-opacity group-hover:opacity-40">
                <Layers size={32} />
                <span class="text-[10px] font-black uppercase tracking-widest">No Result Yet</span>
              </div>
            }
          >
            <Viewport data={resultData()} />
          </Show>
          <div class="absolute bottom-2 right-2 px-1.5 py-0.5 bg-black/60 rounded text-[8px] font-mono text-white/40 pointer-events-none group-hover:text-white/80 transition-colors">
            LOCAL VIEWPORT
          </div>
        </div>
      </Show>
    </div>
  );
};
