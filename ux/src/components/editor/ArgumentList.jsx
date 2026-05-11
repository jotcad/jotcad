import { Index, Show, createMemo } from 'solid-js';
import { Plus, X, ArrowRight, Ban, Check } from 'lucide-solid';

/**
 * ArgumentList: Unified interface for Arguments (Args), Subjects (In), and Ports (Out).
 */
export const ArgumentList = (props) => {
  const items = createMemo(() => {
    const list = [];
    props.args.forEach((a, idx) => {
        list.push({ ...a, kind: a.affiliate ? 'in' : 'arg', originalIndex: idx });
    });
    props.outputs.forEach((o, idx) => {
        list.push({ ...o, kind: 'out', originalIndex: idx });
    });
    return list;
  });

  const addItem = () => {
      props.setArgs([...props.args, { name: 'v' + props.args.length, type: 'jot:number', testValue: 10 }]);
  };

  const removeItem = (item) => {
      if (item.kind === 'out') {
          props.setOutputs(props.outputs.filter((_, i) => i !== item.originalIndex));
      } else {
          props.setArgs(props.args.filter((_, i) => i !== item.originalIndex));
      }
  };

  const updateItem = (item, updates) => {
      if (item.kind === 'out') {
          const next = [...props.outputs];
          next[item.originalIndex] = { ...next[item.originalIndex], ...updates };
          props.setOutputs(next);
      } else {
          const next = [...props.args];
          next[item.originalIndex] = { ...next[item.originalIndex], ...updates };
          props.setArgs(next);
      }
  };

  const cycleRole = (item) => {
      if (item.kind === 'arg') {
          updateItem(item, { affiliate: '$out', name: item.name.startsWith('v') ? '$in' : item.name });
      } else if (item.kind === 'in') {
          const newItem = { name: item.name === '$in' ? '$out' : item.name, type: item.type };
          props.setArgs(props.args.filter((_, i) => i !== item.originalIndex));
          props.setOutputs([...props.outputs, newItem]);
      } else {
          const newItem = { name: item.name === '$out' ? 'v' + props.args.length : item.name, type: item.type, testValue: 10 };
          props.setOutputs(props.outputs.filter((_, i) => i !== item.originalIndex));
          props.setArgs([...props.args, newItem]);
      }
  };

  const types = ['jot:number', 'jot:string', 'jot:shape', 'jot:boolean', 'file'];

  return (
    <div class="flex flex-col bg-black/40 rounded-xl border-2 border-cyan-400 overflow-hidden shadow-2xl">
        {/* Header / Action Bar */}
        <div class="flex items-center justify-between px-3 py-2 bg-cyan-400/5 border-b-2 border-cyan-400/20">
            <span class="text-[10px] font-black tracking-[0.2em] text-cyan-400/60 uppercase">Interface Definition</span>
            <button 
                onClick={addItem} 
                class="flex items-center gap-1.5 px-3 py-1 rounded-full bg-cyan-400/10 text-cyan-400 hover:bg-cyan-400 hover:text-black transition-all text-[10px] font-black uppercase border border-cyan-400/30"
            >
                <Plus size={12} stroke-width={4} />
                Add
            </button>
        </div>

        {/* Unified List */}
        <div class="flex flex-col divide-y divide-cyan-400/10">
            <Index each={items()}>
                {(item) => (
                    <div class="flex gap-2 items-center p-2 hover:bg-cyan-400/5 transition-colors group">
                        
                        {/* Role Cycle Toggle: Square with left/right/empty arrow */}
                        <button 
                            onClick={() => cycleRole(item())}
                            class="w-8 h-8 shrink-0 rounded-lg flex items-center transition-all border-2 border-cyan-400/20 bg-black/40 hover:border-cyan-400/50 active:scale-90"
                            title={`Role: ${item().kind.toUpperCase()} (Click to cycle)`}
                        >
                            <Show when={item().kind === 'in'}>
                                <ArrowRight size={14} stroke-width={4} class="text-amber-400 -ml-1" />
                            </Show>
                            <Show when={item().kind === 'out'}>
                                <div class="w-full flex justify-end">
                                    <ArrowRight size={14} stroke-width={4} class="text-cyan-400 -mr-1" />
                                </div>
                            </Show>
                        </button>

                        {/* Name Input */}
                        <input 
                            class="bg-black/20 border border-white/5 rounded-md px-2 py-1.5 text-[13px] font-bold text-white w-24 focus:outline-none focus:border-cyan-400"
                            value={item().name}
                            placeholder="name"
                            onInput={e => updateItem(item(), { name: e.target.value })}
                        />

                        {/* Type Selector */}
                        <select 
                            class="bg-white/5 border border-white/5 text-[9px] font-black uppercase tracking-tighter text-white/40 focus:outline-none rounded-md px-1.5 py-1.5 appearance-none cursor-pointer hover:text-cyan-400 transition-colors"
                            value={item().type}
                            onChange={e => updateItem(item(), { type: e.target.value })}
                        >
                            <Index each={types}>
                                {(t) => <option value={t()}>{t().split(':')[1] || t()}</option>}
                            </Index>
                        </select>

                        {/* Test Value / Required Area */}
                        <div class="flex-1 flex gap-1.5 items-center min-w-0">
                            <Show when={item().kind !== 'out'} fallback={
                                <div class="flex-1 text-[9px] font-black uppercase text-white/10 tracking-widest text-right pr-2 select-none">
                                    Output Port
                                </div>
                            }>
                                <div class="flex-1 relative">
                                    <input 
                                        disabled={item().testValue === undefined}
                                        class={`bg-black/40 border border-white/5 rounded-md px-2 py-1.5 text-[13px] w-full focus:outline-none focus:border-cyan-400 transition-all ${
                                            item().testValue === undefined ? 'text-white/10 italic' : 'text-amber-400'
                                        }`}
                                        value={item().testValue ?? ''}
                                        placeholder={item().testValue === undefined ? 'Required (No Default)' : (item().type === 'jot:shape' ? 'Expression...' : 'Value')}
                                        onInput={e => {
                                            let val = e.target.value;
                                            if (item().type === 'jot:number') val = parseFloat(val);
                                            updateItem(item(), { testValue: val });
                                        }}
                                    />
                                </div>
                                
                                {/* Required/Optional Toggle */}
                                <button 
                                    onClick={() => updateItem(item(), { testValue: item().testValue === undefined ? (item().type === 'jot:number' ? 0 : '') : undefined })}
                                    class={`w-8 h-8 rounded-lg flex items-center justify-center transition-all border ${
                                        item().testValue === undefined ? 'text-red-400 border-red-400/20 bg-red-400/5' : 'text-green-400/60 border-green-400/10 hover:border-green-400'
                                    }`}
                                    title={item().testValue === undefined ? "Currently Required (Make Optional)" : "Currently Optional (Make Required)"}
                                >
                                    {item().testValue === undefined ? <Ban size={14} stroke-width={3} /> : <Check size={14} stroke-width={3} />}
                                </button>
                            </Show>
                        </div>

                        {/* Delete Action */}
                        <button 
                            onClick={() => removeItem(item())} 
                            class="text-cyan-400/20 group-hover:text-cyan-400 transition-all p-1.5 hover:bg-cyan-400/5 rounded-lg"
                            title="Remove Element"
                        >
                            <X size={18} stroke-width={3} />
                        </button>
                    </div>
                )}
            </Index>
        </div>

        <Show when={items().length === 0}>
            <div class="py-8 text-center text-[9px] font-black uppercase text-white/10 tracking-[0.3em]">
                Interface is Empty
            </div>
        </Show>
    </div>
  );
};
