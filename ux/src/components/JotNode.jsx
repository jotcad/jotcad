import { createSignal, onMount, For, Show, createEffect, createMemo } from 'solid-js';
import interact from 'interactjs';
import { vfs, blackboard, DEFAULT_CODE } from '../lib/blackboard';
import { Selector } from '../../../fs/src/vfs_browser.js';
import { Database, Minus, Maximize2, Layers, Plus, X, Globe, Trash2 } from 'lucide-solid';
import { JotParser } from '../../../jot/src/parser';
import { JotCompiler } from '../../../jot/src/compiler';
import { Viewport } from './Viewport';
import { renderJotToScene, registerViewport, unregisterViewport, initSharedRenderer, requestRender, packZFS } from '../lib/three_utils';

export const JotNode = (props) => {
  let nodeRef;

  const [pos, setPos] = createSignal(props.initial.pos || { x: 400, y: 400 });
  const [size, setSize] = createSignal(props.initial.size || { width: 500, height: 600 });
  const [opName, setOpName] = createSignal(props.initial.opName || 'user/MyOp');
  const [args, setArgs] = createSignal(props.initial.args || [
    { name: 'width', type: 'jot:number', testValue: 20 }
  ]);
  const [code, setCode] = createSignal(props.initial.code || DEFAULT_CODE);
  const [isEvaluating, setIsEvaluating] = createSignal(false);
  const [isMinimized, setIsMinimized] = createSignal(false);
  const [resultData, setResultData] = createSignal(null);
  const [associatedFiles, setAssociatedFiles] = createSignal([]);

  // Sync state back to blackboard
  const syncToBlackboard = () => {
    blackboard.updateEditorState(props.initial.id, {
      pos: pos(),
      size: size(),
      opName: opName(),
      args: args(),
      code: code()
    });
  };

  // Only sync heavy stuff (code/args) automatically or on edit
  createEffect(() => {
    // Track reactive changes and sync
    code(); args(); opName();
    syncToBlackboard();
  });

  onMount(() => {
    interact(nodeRef)
      .draggable({
        allowFrom: '.drag-handle',
        listeners: {
          move(event) {
            setPos({ x: pos().x + event.dx, y: pos().y + event.dy });
            requestRender();
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
            setSize({
              width: event.rect.width,
              height: event.rect.height
            });
            setPos({
              x: pos().x + event.deltaRect.left,
              y: pos().y + event.deltaRect.top
            });
            requestRender();
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

  const addArg = () => setArgs([...args(), { name: 'arg' + args().length, type: 'jot:number', testValue: 10 }]);
  const removeArg = (index) => setArgs(args().filter((_, i) => i !== index));
  const updateArg = (index, field, value) => {
    const next = [...args()];
    next[index] = { ...next[index], [field]: value };
    setArgs(next);
  };

  const closeNode = () => {
    blackboard.closeOp(props.initial.id);
  };

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

  const downloadFile = async (file) => {
    try {
      console.log(`[JotNode] Downloading artifact: ${file.label} from ${file.selector.path}`);
      const data = await vfs.readData(file.selector);
      if (data) {
        const blob = new Blob([data], { type: file.mimeType });
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = file.label;
        a.click();
        URL.revokeObjectURL(url);
      }
    } catch (err) {
      console.error('[JotNode] Download failed:', err);
      alert('Download failed: ' + err.message);
    }
  };

  const evaluateJot = async () => {
    if (isEvaluating()) return;
    setIsEvaluating(true);
    try {
      console.log('[JotNode] Compiling expression...');
      const parser = new JotParser();
      const compiler = new JotCompiler(vfs);

      // 1. Bind UI arguments as variables
      const boundVars = args().reduce((acc, arg) => {
        acc[arg.name] = arg.testValue;
        return acc;
      }, {});

      const currentSchemas = blackboard.schemas();
      // 2. Register all discovered schemas with the compiler
      for (const [path, schema] of Object.entries(currentSchemas)) {
        const name = path.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path, schema });
      }

      const ast = parser.parse(code());
      const terminals = await compiler.evaluate(ast, boundVars);

      // 1. Route Terminals: Shapes to Viewer, Files to Downloads
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
      const primarySelector = shapes[shapes.length - 1]; 

      if (!primarySelector) {
          setResultData(null);
          setIsEvaluating(false);
          return;
      }

      const data = await vfs.readData(primarySelector);
      
      if (data) {
        if (typeof data === 'object' && (data.geometry || data.components)) {
          const unified = await packZFS(vfs, data);
          setResultData(unified);
        } else {
          setResultData(data);
        }
      }
    } catch (err) {
      console.error('[JotNode] Compilation/Evaluation failed:', err);
      blackboard.setError(err);
    } finally {
      setIsEvaluating(false);
    }
  };

  return (
    <div
      ref={nodeRef}
      onPointerDown={() => blackboard.raiseOp(props.initial.id)}
      class="fixed z-50 p-3 md:p-4 rounded-xl border-2 border-white/20 bg-black/80 backdrop-blur-2xl shadow-2xl overflow-hidden flex flex-col gap-2 md:gap-3 transition-all duration-75"
      style={{
        left: `${pos().x}px`,
        top: `${pos().y}px`,
        width: isMinimized() ? '200px' : `${size().width}px`,
        height: isMinimized() ? 'auto' : `${size().height}px`,
        "touch-action": "none"
      }}
    >
      <div
        class="flex justify-between items-center cursor-move drag-handle"
        onDblClick={() => setIsMinimized(!isMinimized())}
      >
        <div class="flex items-center gap-2">
           <input 
             data-testid="op-name-input"
             class="bg-transparent border-none text-xs font-black uppercase tracking-tighter text-cyan-400 focus:outline-none w-32"
             value={opName()}
             onInput={e => setOpName(e.target.value)}
           />
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
          <button
            class="text-white/20 hover:text-red-400 transition-colors"
            onClick={closeNode}
          >
            <X size={14} />
          </button>
        </div>
      </div>

      <Show when={!isMinimized()}>
        {/* Argument Editor */}
        <div class="bg-white/5 rounded-lg p-2 flex flex-col gap-2">
           <div class="flex justify-between items-center px-1">
             <span class="text-[9px] font-black uppercase tracking-widest text-white/40">Arguments</span>
             <button onClick={addArg} class="text-cyan-400 hover:text-cyan-300"><Plus size={12}/></button>
           </div>
           <For each={args()}>
             {(arg, i) => (
               <div class="flex gap-2 items-center">
                 <input 
                   data-testid={`arg-name-${i()}`}
                   class="bg-black/40 border border-white/10 rounded px-1.5 py-0.5 text-[10px] text-white w-20 focus:outline-none focus:border-cyan-400/40"
                   value={arg.name}
                   onInput={e => updateArg(i(), 'name', e.target.value)}
                 />
                 <input 
                   data-testid={`arg-value-${i()}`}
                   type="jot:number"
                   class="bg-black/40 border border-white/10 rounded px-1.5 py-0.5 text-[10px] text-cyan-400 w-16 focus:outline-none focus:border-cyan-400/40"
                   value={arg.testValue}
                   onInput={e => updateArg(i(), 'testValue', parseFloat(e.target.value))}
                 />
                 <button onClick={() => removeArg(i())} class="text-white/20 hover:text-red-400 ml-auto"><X size={10}/></button>
               </div>
             )}
           </For>
        </div>

        <textarea
          class="bg-black/40 border border-white/5 rounded-lg p-3 font-mono text-[11px] min-h-[100px] flex-1 focus:outline-none focus:border-cyan-400/50 text-cyan-200 resize-none custom-scrollbar"
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

        <Show when={associatedFiles().length > 0}>
          <div class="flex flex-wrap gap-2 py-1">
            <For each={associatedFiles()}>
              {(file) => (
                <button
                  onClick={() => downloadFile(file)}
                  class="px-2 py-1 bg-white/10 hover:bg-white/20 border border-white/10 rounded text-[10px] font-mono text-cyan-400 transition-colors flex items-center gap-1"
                >
                  <Database size={10} />
                  {file.label}
                </button>
              )}
            </For>
          </div>
        </Show>

        <div class="w-full min-h-[200px] flex-[1.5] bg-black/20 rounded-lg border border-white/10 overflow-hidden relative group mt-1">
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
