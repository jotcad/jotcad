import { createSignal, onMount, Show, createEffect, createMemo, onCleanup, untrack } from 'solid-js';
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
  
  // 1. CANONICAL SIGNALS (Stored in spatial units)
  const [spatialSize, setSpatialSize] = createSignal(props.data?.size || { width: 500, height: 600 });
  const [pos, setPos] = createSignal(props.data?.pos || { x: 400, y: 400 });
  const [opName, setOpName] = createSignal(props.data?.opName ?? (props.data?.id || ''));
  const [args, setArgs] = createSignal(props.data?.args || [
    { name: '$in', type: 'jot:shape', testValue: 'Box(20)' },
    { name: 'width', type: 'jot:number', testValue: 10 }
  ]);
  const [outputs, setOutputs] = createSignal(props.data?.outputs || [
    { name: '$out', type: 'jot:shape' }
  ]);
  const [code, setCode] = createSignal(props.data?.code || DEFAULT_CODE);
  const [edgeThreshold, setEdgeThreshold] = createSignal(props.data?.edgeThreshold ?? 15);
  const [split, setSplit] = createSignal(props.data?.split || 0.6);

  // 2. TRANSIENT UI SIGNALS
  const [layoutSize, setLayoutSize] = createSignal({ width: 500, height: 600 });
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [results, setResults] = createSignal([]);
  const [associatedFiles, setAssociatedFiles] = createSignal([]);

  const isWide = createMemo(() => layoutSize().width > layoutSize().height * 1.1);

  // 3. SYNCHRONIZATION
  const syncContent = () => {
    if (!props.data?.id) return;
    blackboard.updateEditorState(props.data.id, {
      opName: opName(),
      args: args(),
      outputs: outputs(),
      code: code(),
      edgeThreshold: edgeThreshold(),
      split: split()
    });
  };

  const syncTransform = () => {
    if (!props.data?.id) return;
    blackboard.updateEditorState(props.data.id, {
      pos: pos(),
      size: spatialSize()
    });
  };

  // Sync content whenever it changes (e.g. typing)
  createEffect(() => {
    code(); args(); opName(); edgeThreshold(); split(); outputs();
    untrack(() => syncContent());
  });

  // REACTIVE HYDRATION
  createEffect(() => {
    const data = props.data;
    if (!data) return;
    untrack(() => {
        if (data.pos) setPos(data.pos);
        if (data.size) setSpatialSize(data.size);
        if (data.code && data.code !== code()) setCode(data.code);
        if (data.opName !== undefined) setOpName(data.opName);
        if (data.args) setArgs(data.args);
        if (data.outputs) setOutputs(data.outputs);
        if (data.split) setSplit(data.split);
    });
  });

  onMount(() => {
    const resizeObserver = new ResizeObserver((entries) => {
        for (const entry of entries) {
            setLayoutSize({ width: entry.contentRect.width, height: entry.contentRect.height });
        }
    });
    if (nodeRef) resizeObserver.observe(nodeRef);
    onCleanup(() => resizeObserver.disconnect());

    // Divider dragging logic
    interact(dividerRef).draggable({
        listeners: {
            start(event) {
                if (event.pointerId !== undefined && event.target && event.target.setPointerCapture) {
                    event.target.setPointerCapture(event.pointerId);
                }
            },
            move(event) {
                const rect = nodeRef.getBoundingClientRect();
                const delta = isWide() ? event.dx : event.dy;
                const total = isWide() ? rect.width : rect.height;
                const newSplit = Math.max(0.1, Math.min(0.9, split() + (delta / total)));
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
          end() { syncTransform(); }
        },
      })
      .resizable({
        edges: { left: true, right: true, bottom: true, top: true },
        margin: 12,
        listeners: {
          move(event) {
            if (blackboard.gestureOwner() === 'blackboard') return;
            const s = window._JOT_VIEW ? window._JOT_VIEW().scale : 1;
            setSpatialSize({ width: event.rect.width / s, height: event.rect.height / s });
            setPos({ x: pos().x + event.deltaRect.left / s, y: pos().y + event.deltaRect.top / s });
          },
          end() { syncTransform(); }
        },
        modifiers: [ interact.modifiers.restrictSize({ min: { width: 350, height: 400 } }) ]
      });
  });

  const isUnpublished = createMemo(() => {
    const path = opName();
    if (!path) return true;
    const published = blackboard.dynamicOps()[path];
    if (!published) return true;
    const local = code().trim();
    const remote = (published.script || '').trim();
    return local !== remote;
  });

  const publishToMesh = () => {
     let path = opName().trim();
     if (path && !path.includes('/')) {
         path = `user/${path}`;
         setOpName(path);
         blackboard.updateEditorState(props.data.id, { opName: path, label: path });
     }
     if (!path || !path.startsWith('user/')) {
         alert("Please enter a valid operator path (e.g. 'user/MyOperator')");
         return;
     }
     if (props.data.id.startsWith('new-op-')) {
         blackboard.rename(props.data.id, path);
     }

     const schema = {
       path,
       arguments: args().map(arg => ({ name: arg.name, type: arg.type, default: arg.testValue })),
       outputs: outputs().reduce((acc, out) => {
           acc[out.name] = { type: out.type };
           return acc;
       }, {})
     };
     blackboard.publishDynamicOp(path, schema, code());
  };

  const evaluateJot = async () => {
    if (isEvaluating()) return;
    setIsEvaluating(true);
    try {
      const parser = new JotParser();
      const compiler = new JotCompiler(vfs);

      const currentSchemas = blackboard.schemas();
      for (const [path, schema] of Object.entries(currentSchemas)) {
        const name = path.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path, schema });
      }

      // --- SCHEMA-DRIVEN EVALUATION ---
      const boundVars = {};
      for (const arg of args()) {
          if (arg.type === 'jot:shape' || arg.type === 'shape') {
              if (arg.testValue && typeof arg.testValue === 'string') {
                  const subAst = parser.parse(arg.testValue);
                  const res = await compiler.evaluate(subAst, {});
                  boundVars[arg.name] = Array.isArray(res) ? res[0] : res;
              } else {
                  boundVars[arg.name] = arg.testValue;
              }
          } else {
              boundVars[arg.name] = arg.testValue;
          }
      }

      const ast = parser.parse(code());
      
      // Construct local test schema for extraction
      const testSchema = {
          outputs: outputs().reduce((acc, out) => { acc[out.name] = { type: out.type }; return acc; }, {})
      };

      const terminals = await compiler.evaluate(ast, boundVars, testSchema);

      const shapes = [];
      const files = [];

      for (const sel of terminals) {
        const portName = sel.output || '$out';
        const portDef = testSchema.outputs[portName];
        const type = portDef?.type || 'jot:shape';

        if (type === 'jot:shape' || type === 'shape') {
          shapes.push(sel);
        } else if (type === 'file') {
          files.push({ label: sel.parameters.path || portName, selector: sel, mimeType: 'application/pdf' });
        }
      }

      setAssociatedFiles(files);

      const resultList = await Promise.all(shapes.map(async (sel) => {
        const data = await vfs.readData(sel);
        let finalData = data;
        if (data && typeof data === 'object' && (data.geometry || data.components)) {
          finalData = await packZFS(vfs, data);
        }
        return { selector: sel, data: finalData, port: sel.output };
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
    <div ref={nodeRef} class={`jot-node flex h-full gap-1.5 p-1.5 bg-transparent overflow-hidden ${isWide() ? 'flex-row' : 'flex-col'}`}>
      
      <div 
        class="flex flex-col gap-1.5 shrink-0 overflow-hidden"
        style={{ 
            [isWide() ? 'width' : 'height']: `${split() * 100}%`,
            [isWide() ? 'min-width' : 'min-height']: isWide() ? "300px" : "180px"
        }}
      >
          <Show when={!props.isWindowed}>
              <div class="flex justify-between items-center cursor-move drag-handle shrink-0 mb-1 px-1">
                <div class="flex items-center gap-2">
                   <input 
                     class="bg-transparent border-none text-ui-label font-black tracking-tighter text-cyan-400 focus:outline-none w-48 placeholder:text-cyan-400/20"
                     value={opName()}
                     placeholder="Name your op..."
                     onInput={e => setOpName(e.target.value)}
                     onKeyDown={e => { if (e.key === 'Enter') e.currentTarget.blur(); }}
                   />
                </div>
                <div class="flex items-center gap-2">
                  <button onClick={publishToMesh} class={`relative transition-all p-1 ${isUnpublished() ? 'text-cyan-400' : 'text-white/20'}`} title="Publish Operator">
                    <Globe size={14} />
                    <div class={`absolute top-0 right-0 w-1.5 h-1.5 rounded-full ${isUnpublished() ? 'bg-red-500' : 'bg-green-500'}`} />
                  </button>
                  <button 
                    onClick={() => { if (confirm(`Delete operator '${opName()}' from library?`)) { blackboard.removeDynamicOp(opName()); blackboard.closeOp(props.data.id); } }}
                    class="text-white/30 hover:text-red-400 transition-colors p-1" title="Delete Operator"
                  >
                    <Trash2 size={14} />
                  </button>
                  <button onClick={() => blackboard.closeOp(props.data.id)} class="text-white/30 hover:text-cyan-400 transition-colors p-1">
                    <X size={14} />
                  </button>
                </div>
              </div>
          </Show>

          <div class="flex-1 flex flex-col gap-2 min-h-0">
              <div class="shrink-0 max-h-48 overflow-y-auto custom-scrollbar">
                  <ArgumentList args={args()} setArgs={setArgs} outputs={outputs()} setOutputs={setOutputs} />
              </div>
    
              <textarea
                class="flex-1 bg-black/80 border border-white/10 rounded-lg p-3 font-mono text-readable focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar shadow-inner"
                value={code()}
                onInput={(e) => setCode(e.target.value)}
                onKeyDown={(e) => { if (e.shiftKey && e.key === 'Enter') { e.preventDefault(); evaluateJot(); } }}
                spellcheck={false}
              />

              <div class="flex gap-2 shrink-0">
                <button
                  onClick={evaluateJot}
                  disabled={isEvaluating()}
                  class={`flex-1 py-2 rounded-full text-xs font-black uppercase tracking-widest transition-all border-2 ${
                    isEvaluating() 
                      ? 'bg-cyan-500/5 text-cyan-500/40 border-cyan-500/10' 
                      : 'bg-cyan-500/10 text-cyan-400 border-cyan-400 hover:bg-cyan-500/20 hover:shadow-[0_0_15px_rgba(34,211,238,0.2)] active:scale-95'
                  }`}
                >
                  {isEvaluating() ? 'EVALUATING...' : 'Evaluate Jot'}
                </button>
              </div>
          </div>
      </div>

      <div 
        ref={dividerRef}
        class={`flex items-center justify-center group shrink-0 touch-none ${isWide() ? 'w-6 h-full cursor-col-resize' : 'h-6 w-full cursor-row-resize'}`}
      >
          <div class={`rounded-full bg-cyan-400 shadow-[0_0_15px_rgba(34,211,238,0.4)] border border-white/20 transition-all ${isWide() ? 'w-2 h-24 group-hover:scale-y-110' : 'w-24 h-2 group-hover:scale-x-110'}`} />
      </div>

      <div class="flex-1 min-h-0 flex flex-col overflow-hidden">
          <ResultList results={results()} files={associatedFiles()} edgeThreshold={edgeThreshold()} setEdgeThreshold={setEdgeThreshold} />
      </div>
    </div>
  );

  return (
    <Show when={props.isWindowed} fallback={
        <div
          onPointerDown={() => props.data && blackboard.raiseOp(props.data.id)}
          class="jot-node absolute p-3 md:p-4 rounded-2xl border-2 border-cyan-400 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-y-auto flex flex-col gap-2 md:gap-3 transition-all duration-75 custom-scrollbar"
          style={{
            left: `${pos().x}px`,
            top: `${pos().y}px`,
            width: `${spatialSize().width}px`,
            height: `${spatialSize().height}px`,
            "z-index": props.data?.zIndex || 0
          }}
        >
            {content()}
        </div>
    }>
        {content()}
    </Show>
  );
};
