import { createSignal, onMount, Show, createEffect, createMemo } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard, DEFAULT_CODE } from '../../lib/blackboard';
import { Minus, Maximize2, X, Globe, Trash2, Zap } from 'lucide-solid';
import { JotParser } from '../../../../jot/src/parser';
import { JotCompiler } from '../../../../jot/src/compiler';
import { packZFS } from '../../lib/render/GeometryDecoder';

import { ArgumentList } from './ArgumentList';
import { ResultList } from './ResultList';

export const JotNode = (props) => {
  let nodeRef;
  let dividerRef;
  const [pos, setPos] = createSignal(props.data?.pos || { x: 400, y: 400 });
  const [size, setSize] = createSignal(props.data?.size || { width: 500, height: 600 });
  const [opName, setOpName] = createSignal(props.data?.opName || props.data?.id || 'user/MyOp');
  const [args, setArgs] = createSignal(props.data?.args || [
    { name: 'width', type: 'jot:number', testValue: 20 }
  ]);
  const [code, setCode] = createSignal(props.data?.code || DEFAULT_CODE);
  const [edgeThreshold, setEdgeThreshold] = createSignal(props.data?.edgeThreshold ?? 15);
  const [split, setSplit] = createSignal(props.data?.split || 0.6); // 60% editor by default
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [results, setResults] = createSignal([]);
  const [associatedFiles, setAssociatedFiles] = createSignal([]);

  const syncToBlackboard = () => {
    if (!props.data?.id) return;
    blackboard.updateEditorState(props.data.id, {
      pos: pos(),
      size: size(),
      opName: opName(),
      args: args(),
      code: code(),
      edgeThreshold: edgeThreshold(),
      split: split()
    });
  };

  createEffect(() => {
    code(); args(); opName(); edgeThreshold(); split();
    syncToBlackboard();
  });

  onMount(() => {
    // Divider dragging logic
    interact(dividerRef).draggable({
        lockAxis: 'y',
        listeners: {
            move(event) {
                const rect = nodeRef.getBoundingClientRect();
                const totalHeight = rect.height;
                const delta = event.dy;
                const newSplit = Math.max(0.1, Math.min(0.9, split() + (delta / totalHeight)));
                setSplit(newSplit);
            }
        }
    });

    if (props.isWindowed) return;

    interact(nodeRef)
      .draggable({
        allowFrom: '.drag-handle',
        listeners: {
          move(event) {
            if (blackboard.gestureOwner() === 'blackboard') return;
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            setPos({ x: pos().x + event.dx / s, y: pos().y + event.dy / s });
          },
          end() { syncToBlackboard(); }
        },
      })
      .resizable({
        edges: { left: false, right: true, bottom: true, top: false },
        listeners: {
          move(event) {
            if (blackboard.gestureOwner() === 'blackboard') return;
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            setSize({ width: event.rect.width / s, height: event.rect.height / s });
            setPos({ x: pos().x + event.deltaRect.left / s, y: pos().y + event.deltaRect.top / s });
          },
          end() { syncToBlackboard(); }
        },
        modifiers: [ interact.modifiers.restrictSize({ min: { width: 350, height: 400 } }) ]
      });
  });

  const isUnpublished = createMemo(() => {
    const published = blackboard.dynamicOps()[opName()];
    if (!published) return true;
    return published.script !== code();
  });

  const publishToMesh = () => {
     const schema = {
       path: opName(),
       arguments: args().map(arg => ({
         name: arg.name,
         type: arg.type,
         default: arg.testValue
       })),
       outputs: { "$out": { type: "shape" } }
     };
     blackboard.publishDynamicOp(opName(), schema, code());
  };

  const evaluateJot = async () => {
    if (isEvaluating()) return;
    setIsEvaluating(true);
    try {
      const parser = new JotParser();
      const compiler = new JotCompiler(vfs);

      const boundVars = args().reduce((acc, arg) => {
        acc[arg.name] = arg.testValue;
        return acc;
      }, {});

      const currentSchemas = blackboard.schemas();
      for (const [path, schema] of Object.entries(currentSchemas)) {
        const name = path.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path, schema });
      }

      const ast = parser.parse(code());
      const terminals = await compiler.evaluate(ast, boundVars);

      const shapes = [];
      const files = [];

      for (const sel of terminals) {
        const schema = currentSchemas[sel.path];
        const output = schema?.outputs?.[sel.output || '$out'];
        const type = output?.type || 'jot:shape';

        if (type === 'jot:shape' || type === 'shape') {
          shapes.push(sel);
        } else if (type === 'file') {
          const label = sel.parameters.path || sel.output;
          files.push({
            label,
            selector: sel,
            mimeType: output?.mimeType || 'application/octet-stream'
          });
        }
      }

      setAssociatedFiles(files);

      const resultList = await Promise.all(shapes.map(async (sel) => {
        const data = await vfs.readData(sel);
        let finalData = data;
        if (data && typeof data === 'object' && (data.geometry || data.components)) {
          finalData = await packZFS(vfs, data);
        }
        return { selector: sel, data: finalData };
      }));

      setResults(resultList);
    } catch (err) {
      console.error('[JotNode] Evaluation failed:', err);
      blackboard.setError(err);
    } finally {
      setIsEvaluating(false);
    }
  };

  const content = () => (
    <div ref={nodeRef} class="jot-node flex flex-col h-full gap-1.5 p-1.5 bg-transparent overflow-hidden">
      <Show when={!props.isWindowed}>
          <div class="flex justify-between items-center cursor-move drag-handle shrink-0 mb-1 px-1">
            <div class="flex items-center gap-2">
               <input 
                 class="bg-transparent border-none text-ui-label font-black uppercase tracking-tighter text-cyan-400 focus:outline-none w-48"
                 value={opName()}
                 onInput={e => setOpName(e.target.value)}
               />
            </div>
            <div class="flex items-center gap-2">
              <button onClick={() => blackboard.closeOp(props.data.id)} class="text-white/30 hover:text-red-400 transition-colors p-1">
                <X size={14} />
              </button>
            </div>
          </div>
      </Show>

      {/* Top Group: Arguments + Editor + Actions */}
      <div 
        class="flex flex-col gap-2 shrink-0 overflow-hidden" 
        style={{ height: `${split() * 100}%`, "min-height": "150px" }}
      >
          <ArgumentList args={args()} setArgs={setArgs} />

          <textarea
            class="flex-1 bg-black/80 border border-white/10 rounded-lg p-3 font-mono text-readable focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar shadow-inner"
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

          <div class="flex gap-2 shrink-0">
            <button
              onClick={evaluateJot}
              disabled={isEvaluating()}
              class={`flex-1 py-2 rounded-lg text-xs font-bold transition-all border ${
                isEvaluating() ? 'bg-cyan-900/40 text-cyan-500 border-cyan-500/20' : 'bg-cyan-400 text-black border-cyan-400 hover:bg-cyan-300 active:scale-95'
              }`}
            >
              {isEvaluating() ? 'EVALUATING...' : 'EVALUATE JOT'}
            </button>
            <button
              onClick={publishToMesh}
              class={`px-3 rounded-lg transition-all flex items-center justify-center border shadow-lg ${
                isUnpublished() ? 'bg-orange-500 text-black border-orange-400 hover:bg-orange-400' : 'bg-white/10 text-white border-white/10 hover:bg-white/20'
              }`}
              title={isUnpublished() ? 'Publish Changes' : 'Published'}
            >
              <Globe size={18} />
            </button>
          </div>
      </div>

      {/* Sliding Divider */}
      <div 
        ref={dividerRef}
        class="h-4 w-full cursor-row-resize flex items-center justify-center group shrink-0"
      >
          <div class="w-16 h-1.5 rounded-full bg-white/10 group-hover:bg-cyan-400 transition-colors shadow-lg border border-white/5" />
      </div>

      {/* Bottom Group: Results */}
      <div class="flex-1 overflow-auto">
          <ResultList 
            results={results()} 
            files={associatedFiles()} 
            edgeThreshold={edgeThreshold()} 
            setEdgeThreshold={setEdgeThreshold} 
          />
      </div>
    </div>
  );

  return (
    <Show when={props.isWindowed} fallback={
        <div
          onPointerDown={() => blackboard.raiseOp(props.data.id)}
          class="jot-node absolute p-3 md:p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-y-auto flex flex-col gap-2 md:gap-3 transition-all duration-75 custom-scrollbar"
          style={{
            left: `${pos().x}px`,
            top: `${pos().y}px`,
            width: `${size().width}px`,
            height: `${size().height}px`,
            "z-index": props.data.zIndex
          }}
        >
            {content()}
        </div>
    }>
        {content()}
    </Show>
  );
};
