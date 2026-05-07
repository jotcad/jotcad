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
    <div class="bg-white/5 rounded-lg p-2 flex flex-col gap-2 shrink-0">
       <div class="flex justify-between items-center px-1">
         <span class="text-[9px] font-black uppercase tracking-widest text-white/40">Arguments</span>
         <button onClick={addArg} class="text-cyan-400 hover:text-cyan-300"><Plus size={12}/></button>
       </div>
       <For each={props.args}>
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
  );
};
