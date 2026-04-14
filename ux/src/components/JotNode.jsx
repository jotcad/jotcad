import { createSignal, onMount, For, Show } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard } from '../lib/blackboard';
import { Database, Minus, Maximize2 } from 'lucide-solid';
import { JotParser } from '../../../jot/src/parser';
import { JotCompiler } from '../../../jot/src/compiler';

const DEFAULT_CODE = `
// JotCAD Expressions
hexagon({ radius: 30, variant: 'full' })
`.trim();

export const JotNode = (props) => {
  let nodeRef;
  const [pos, setPos] = createSignal({ x: 400, y: 400 });
  const [code, setCode] = createSignal(DEFAULT_CODE);
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(false);

  const parser = new JotParser();
  const compiler = new JotCompiler();

  onMount(() => {
    interact(nodeRef).draggable({
      listeners: {
        move(event) {
          setPos({ x: pos().x + event.dx, y: pos().y + event.dy });
        },
      },
    });
  });

  const evaluateJot = async () => {
    if (isEvaluating()) return;
    setIsEvaluating(true);
    try {
      console.log('[JotNode] Compiling expression...');
      
      // Register all discovered schemas with the compiler
      const currentSchemas = blackboard.schemas();
      for (const [path, schema] of Object.entries(currentSchemas)) {
          // Extract the last part of the path as the "function name"
          const name = path.split('/').pop();
          compiler.registerOperator(name, { path, schema });
      }

      const ast = parser.parse(code());
      const selector = compiler.evaluate(ast);
      
      console.log('[JotNode] Translated to selector:', JSON.stringify(selector, null, 2));

      // Handle Sequences (Arrays)
      const selectors = Array.isArray(selector) ? selector : [selector];
      
      for (const s of selectors) {
          const stream = await vfs.read(s.path, s.parameters);
          
          if (!stream) {
              console.warn(`[JotNode] Mesh resolution failed: No provider found for ${s.path}`);
              continue;
          }

          // Convert stream to readable text for logging
          const reader = stream.getReader();
          const result = await reader.read();
          const textDecoder = new TextDecoder();
          const resultString = result.value ? textDecoder.decode(result.value) : 'Empty result';
          
          // For now, only the last result is saved to this specific path
          await vfs.writeData('ui/result/jot_eval', {}, result.value || new Uint8Array(0));
          console.log(`[JotNode] Result for ${s.path} saved. Content:`, resultString);
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
        width: isMinimized() ? '200px' : '400px',
        'z-index': 20,
        transition: 'width 0.2s, height 0.2s'
      }}
    >
      <div 
        class="flex justify-between items-center cursor-move"
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="text-xs font-black uppercase tracking-tighter text-white/40">Jot Engine Expression</div>
        <div class="flex items-center gap-2">
            <div class={`w-2 h-2 rounded-full ${isEvaluating() ? 'bg-cyan-400 animate-pulse' : 'bg-white/10'}`}></div>
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
      </Show>
    </div>
  );
};
