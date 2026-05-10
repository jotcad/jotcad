import { For, Show } from 'solid-js';
import { Plus, X } from 'lucide-solid';

export const ArgumentList = (props) => {
  const addArg = () => props.setArgs([...props.args, { name: 'arg' + props.args.length, type: 'jot:number', testValue: 10 }]);
  const removeArg = (index) => props.setArgs(props.args.filter((_, i) => i !== index));
  const updateArg = (index, field, value) => {
    const next = [...props.args];
    next[index] = { ...next[index], [field]: value };
    props.setArgs(next);
  };

  return (
    <div class="bg-white/5 rounded-lg p-3 flex flex-col gap-3 shrink-0">
       <div class="flex justify-between items-center px-1">
         <span class="text-ui-label font-black uppercase tracking-widest text-white/40">Arguments</span>
         <button onClick={addArg} class="tap-target text-cyan-400 hover:text-cyan-300 flex items-center justify-center p-1">
           <Plus size={20} stroke-width={3} />
         </button>
       </div>
       <For each={props.args}>
         {(arg, i) => (
           <div class="flex gap-3 items-center">
             <input 
               data-testid={`arg-name-${i()}`}
               class="bg-black/40 border border-white/10 rounded-lg px-3 py-2 text-readable text-white w-32 focus:outline-none focus:border-cyan-400/40"
               value={arg.name}
               onInput={e => updateArg(i(), 'name', e.target.value)}
             />
             <input 
               data-testid={`arg-value-${i()}`}
               type="jot:number"
               class="bg-black/40 border border-white/10 rounded-lg px-3 py-2 text-readable text-cyan-400 w-24 focus:outline-none focus:border-cyan-400/40"
               value={arg.testValue}
               onInput={e => updateArg(i(), 'testValue', parseFloat(e.target.value))}
             />
             <button onClick={() => removeArg(i())} class="tap-target text-white/20 hover:text-red-400 ml-auto flex items-center justify-center p-1">
               <X size={18} stroke-width={3} />
             </button>
           </div>
         )}
       </For>
    </div>
  );
};
