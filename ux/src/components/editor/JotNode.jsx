import { createSignal, onMount, Show, createEffect, createMemo } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard, DEFAULT_CODE } from '../../lib/blackboard';
import { Minus, Maximize2, X, Globe, Trash2 } from 'lucide-solid';
import { JotParser } from '../../../../jot/src/parser';
import { JotCompiler } from '../../../../jot/src/compiler';
import { packZFS } from '../../lib/render/GeometryDecoder';

import { ArgumentList } from './ArgumentList';
import { ResultList } from './ResultList';

export const JotNode = (props) => {
  let nodeRef;

  const [pos, setPos] = createSignal(props.initial?.pos || { x: 400, y: 400 });
  const [size, setSize] = createSignal(props.initial?.size || { width: 500, height: 600 });
  const [opName, setOpName] = createSignal(props.initial?.opName || 'user/MyOp');
  const [args, setArgs] = createSignal(props.initial?.args || [
    { name: 'width', type: 'jot:number', testValue: 20 }
  ]);
  const [code, setCode] = createSignal(props.initial?.code || DEFAULT_CODE);
  const [edgeThreshold, setEdgeThreshold] = createSignal(props.initial?.edgeThreshold ?? 15);
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(false);
  const [results, setResults] = createSignal([]);
  const [associatedFiles, setAssociatedFiles] = createSignal([]);

  const syncToBlackboard = () => {
    if (!props.initial?.id) return;
    blackboard.updateEditorState(props.initial.id, {
      pos: pos(),
      size: size(),
      opName: opName(),
      args: args(),
      code: code(),
      edgeThreshold: edgeThreshold()
    });
  };

  createEffect(() => {
    code(); args(); opName(); edgeThreshold();
    syncToBlackboard();
  });

  onMount(() => {
    interact(nodeRef)
      .draggable({
        allowFrom: '.drag-handle',
        listeners: {
          move(event) {
            // Inhibit if blackboard owns the gesture
            if (blackboard.gestureOwner() === 'blackboard') return;
            
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            setPos({ x: pos().x + event.dx / s, y: pos().y + event.dy / s });
          },
          end() {
            syncToBlackboard();
          }
        },
      })
      .resizable({
        edges: { left: false, right: true, bottom: true, top: false },
        listeners: {
          move(event) {
            // Inhibit if blackboard owns the gesture
            if (blackboard.gestureOwner() === 'blackboard') return;

            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            setSize({
              width: event.rect.width / s,
              height: event.rect.height / s
            });
            setPos({
              x: pos().x + event.deltaRect.left / s,
              y: pos().y + event.deltaRect.top / s
            });
          },
          end() {
            syncToBlackboard();
          }
        },
        modifiers: [
          interact.modifiers.restrictSize({
            min: { width: 350, height: 400 }
          })
        ]
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

  const handleRaise = (e) => {
    if (e.pointerType === 'mouse') {
      blackboard.raiseOp(props.initial.id);
    } else {
      // For touch, wait to see if a second finger is added
      setTimeout(() => {
        // Only raise if it's still a single-finger interaction globally
        // AND we haven't entered 'Gesturing' mode (which happens at count=2)
        if (blackboard.pointerCount() === 1 && e.isPrimary && !blackboard.isGesturing()) {
           blackboard.raiseOp(props.initial.id);
        }
      }, 100);
    }
  };

  return (
    <div
      ref={nodeRef}
      onPointerDown={handleRaise}
      data-id={props.initial.id}
      class="jot-node absolute z-50 p-3 md:p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-y-auto flex flex-col gap-2 md:gap-3 transition-all duration-75 custom-scrollbar"
      style={{
        left: `${pos().x}px`,
        top: `${pos().y}px`,
        width: isMinimized() ? '200px' : `${size().width}px`,
        height: isMinimized() ? 'auto' : `${size().height}px`
      }}
    >
      <div
        class="flex justify-between items-center cursor-move drag-handle shrink-0"
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="flex items-center gap-2">
           <input 
             data-testid="op-name-input"
             class="bg-transparent border-none text-xs font-black uppercase tracking-tighter text-cyan-400 focus:outline-none w-32"
             value={opName()}
             onInput={e => setOpName(e.target.value)}
             onFocus={() => blackboard.raiseOp(props.initial.id)}
           />
        </div>
        <div class="flex items-center gap-2">
          <div
            class={`w-2.5 h-2.5 rounded-full ${
              isEvaluating() ? 'bg-cyan-400 animate-pulse' : 'bg-white/10'
            }`}
          ></div>
          <button
            class="text-white/20 hover:text-white transition-colors"
            onClick={() => setIsMinimized(!isMinimized())}
          >
            {isMinimized() ? <Maximize2 size={12} /> : <Minus size={12} />}
          </button>
          <button
            class="text-white/20 hover:text-red-400 transition-colors"
            onClick={() => blackboard.closeOp(props.initial.id)}
          >
            <X size={14} />
          </button>
        </div>
      </div>

      <Show when={!isMinimized()}>
        <ArgumentList args={args()} setArgs={setArgs} />

        <textarea
          class="bg-black/40 border border-white/5 rounded-lg p-3 font-mono text-[11px] min-h-[150px] focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar shrink-0"
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
            class={`flex-1 py-2 rounded-lg text-sm font-bold transition-all ${
              isEvaluating()
                ? 'bg-cyan-500/20 text-cyan-400'
                : 'bg-cyan-400 text-black hover:bg-cyan-300'
            }`}
          >
            {isEvaluating() ? 'EVALUATING...' : 'EVALUATE JOT'}
          </button>
          <button
            onClick={publishToMesh}
            class={`px-3 rounded-lg transition-all flex items-center justify-center ${
              isUnpublished() ? 'bg-orange-500/20 text-orange-400 hover:bg-orange-500/40' : 'bg-white/10 text-white/60 hover:bg-white/20'
            }`}
            title={isUnpublished() ? 'Publish Changes to Mesh' : 'Published to Mesh'}
          >
            <Globe size={16} />
          </button>
          <button
            onClick={() => blackboard.clearStorage()}
            class="px-3 rounded-lg bg-white/10 hover:bg-red-500/20 text-white/60 hover:text-red-400 transition-all flex items-center justify-center"
            title="Clear Mesh Storage & Reset"
          >
            <Trash2 size={16} />
          </button>
        </div>

        <ResultList 
          results={results()} 
          files={associatedFiles()} 
          edgeThreshold={edgeThreshold()} 
          setEdgeThreshold={setEdgeThreshold} 
        />
      </Show>
    </div>
  );
};
