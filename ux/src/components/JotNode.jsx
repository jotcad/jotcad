import { createSignal, onMount, For, Show } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard } from '../lib/blackboard';
import { Database } from 'lucide-solid';
import { JotParser } from '../../../jot/src/parser';
import { JotEvaluator } from '../../../jot/src/evaluator';

const DEFAULT_CODE = `
// JotCAD Expressions
hexagon({ radius: 30, variant: 'full' })
`.trim();

export const JotNode = (props) => {
  let nodeRef;
  const [pos, setPos] = createSignal({ x: 400, y: 400 });
  const [code, setCode] = createSignal(DEFAULT_CODE);
  const [isEvaluating, setIsEvaluating] = createSignal(false);

  const parser = new JotParser();
  const evaluator = new JotEvaluator();

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
      const ast = parser.parse(code());
      const selector = evaluator.resolve(ast);
      
      console.log('[JotNode] Translated to selector:', JSON.stringify(selector, null, 2));

      const stream = await vfs.read(selector.path, selector.parameters);
      
      if (!stream) {
          throw new Error('Mesh resolution failed: No provider found for ' + selector.path);
      }

      // Convert stream to readable text for logging
      const reader = stream.getReader();
      const result = await reader.read();
      const textDecoder = new TextDecoder();
      const resultString = result.value ? textDecoder.decode(result.value) : 'Empty result';
      
      await vfs.write('ui/result/jot_eval', {}, result.value || new Uint8Array(0));
      console.log('[JotNode] Result saved to ui/result/jot_eval. Content:', resultString);
      
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
        transform: `translate(${pos().x}px, ${pos().y}px)`,
        width: '400px',
        'z-index': 20
      }}
    >
      <div class="flex justify-between items-center">
        <div class="text-xs font-black uppercase tracking-tighter text-white/40">Jot Engine Expression</div>
        <div class={`w-2 h-2 rounded-full ${isEvaluating() ? 'bg-cyan-400 animate-pulse' : 'bg-white/10'}`}></div>
      </div>

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
    </div>
  );
};
