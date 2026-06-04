import { createSignal, Show, For } from 'solid-js';
import { Palette, Plus, Edit, Trash2 } from 'lucide-solid';
import { paletteSignal, registerMaterial, unregisterMaterial } from '../../lib/render/AssetManager';

export const MaterialsApp = (props) => {
    const [formName, setFormName] = createSignal('');
    const [formValue, setFormValue] = createSignal('');
    const [editingName, setEditingName] = createSignal(null);

    const handleStartEdit = (name, val) => {
        setEditingName(name);
        setFormName(name);
        setFormValue(val);
    };

    const handleCancelEdit = () => {
        setEditingName(null);
        setFormName('');
        setFormValue('');
    };

    const handleSubmit = () => {
        const name = formName().trim();
        const val = formValue().trim();
        if (!name || !val) {
            alert('Please enter both name and URL/CID.');
            return;
        }
        registerMaterial(name, val);
        handleCancelEdit();
    };

    const handleRemove = (name) => {
        if (confirm(`Remove material mapping for "${name}"?`)) {
            unregisterMaterial(name);
            if (editingName() === name) {
                handleCancelEdit();
            }
        }
    };

    return (
        <div class={props.isWindowed ? "w-full h-full bg-slate-900/50 flex flex-col p-6 md:p-10 gap-6 overflow-y-auto custom-scrollbar" : "flex flex-col gap-6"}>
            <div class="flex items-center gap-3 text-cyan-400">
                <Palette size={24} />
                <h2 class="text-ui-label font-black uppercase tracking-[0.2em]">Material Textures</h2>
            </div>
            
            <div class="bg-black/60 border border-white/10 rounded-2xl p-6 flex flex-col gap-6 shadow-xl">
                <div class="text-white/60 text-readable">
                    Map logical material names (e.g. <code>steel</code>, <code>wood</code>) to texture image URLs or content CIDs. These mappings are saved persistently.
                </div>

                <div class="flex flex-col gap-4 max-h-[300px] overflow-y-auto pr-2 custom-scrollbar">
                    <For each={Object.entries(paletteSignal())}>
                        {([name, val]) => (
                            <div class="flex items-center justify-between gap-4 p-3 bg-white/5 border border-white/5 rounded-xl hover:border-cyan-500/30 transition-all">
                                <div class="flex flex-col min-w-0 flex-1 gap-1">
                                    <span class="font-bold text-white/95 uppercase tracking-wider text-sm font-mono">{name}</span>
                                    <span class="text-xs text-white/40 truncate font-mono select-all" title={val}>{val}</span>
                                </div>
                                <div class="flex items-center gap-2">
                                    <button
                                        onClick={() => handleStartEdit(name, val)}
                                        class="p-2 rounded-lg bg-white/5 text-white/70 hover:bg-cyan-500/20 hover:text-cyan-400 transition-all"
                                        title="Edit mapping"
                                    >
                                        <Edit size={16} />
                                    </button>
                                    <button
                                        onClick={() => handleRemove(name)}
                                        class="p-2 rounded-lg bg-white/5 text-white/50 hover:bg-red-500/20 hover:text-red-400 transition-all"
                                        title="Delete mapping"
                                    >
                                        <Trash2 size={16} />
                                    </button>
                                </div>
                            </div>
                        )}
                    </For>
                </div>

                {/* Add / Edit Form */}
                <div class="border-t border-white/10 pt-6 flex flex-col gap-4">
                    <div class="text-sm font-bold text-white/80 uppercase tracking-widest">
                        {editingName() ? `Edit Material: ${editingName()}` : 'Add New Material Mapping'}
                    </div>
                    <div class="flex flex-col md:flex-row gap-3">
                        <input 
                            type="text"
                            placeholder="Material Name (e.g. brass)"
                            value={formName()}
                            onInput={(e) => setFormName(e.target.value)}
                            disabled={!!editingName()}
                            class="flex-1 md:flex-initial md:w-[200px] px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all font-mono"
                        />
                        <input 
                            type="text"
                            placeholder="Texture Web URL or CID"
                            value={formValue()}
                            onInput={(e) => setFormValue(e.target.value)}
                            class="flex-1 px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all font-mono"
                        />
                        <div class="flex gap-2">
                            <button
                                onClick={handleSubmit}
                                class="flex-1 md:flex-none px-6 py-3 rounded-xl bg-cyan-500 text-slate-950 font-black uppercase tracking-widest hover:bg-cyan-400 hover:shadow-[0_0_20px_rgba(34,211,238,0.4)] active:scale-95 transition-all flex items-center justify-center gap-2"
                            >
                                <Plus size={18} />
                                {editingName() ? 'Save' : 'Add'}
                            </button>
                            <Show when={editingName()}>
                                <button
                                    onClick={handleCancelEdit}
                                    class="px-4 py-3 rounded-xl bg-white/5 border border-white/10 text-white/70 hover:bg-white/10 hover:text-white transition-all text-sm font-bold uppercase tracking-wider"
                                >
                                    Cancel
                                </button>
                            </Show>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    );
};
