import { createSignal, Show, For } from 'solid-js';
import { Tag, Plus, Edit, Trash2 } from 'lucide-solid';
import { skuCatalogSignal, registerSku, unregisterSku } from '../../lib/render/AssetManager';

export const SkusApp = (props) => {
    const [formSku, setFormSku] = createSignal('');
    const [formPrice, setFormPrice] = createSignal('');
    const [formSupplier, setFormSupplier] = createSignal('');
    const [formDesc, setFormDesc] = createSignal('');
    const [editingSku, setEditingSku] = createSignal(null);

    const handleStartEditSku = (sku, meta) => {
        setEditingSku(sku);
        setFormSku(sku);
        setFormPrice(meta.price || '');
        setFormSupplier(meta.supplier || '');
        setFormDesc(meta.description || '');
    };

    const handleCancelEditSku = () => {
        setEditingSku(null);
        setFormSku('');
        setFormPrice('');
        setFormSupplier('');
        setFormDesc('');
    };

    const handleSubmitSku = () => {
        const sku = formSku().trim();
        const price = formPrice().trim();
        const supplier = formSupplier().trim();
        const description = formDesc().trim();
        if (!sku) {
            alert('Please enter a SKU name.');
            return;
        }
        registerSku(sku, { price, supplier, description });
        handleCancelEditSku();
    };

    const handleRemoveSku = (sku) => {
        if (confirm(`Remove SKU record for "${sku}"?`)) {
            unregisterSku(sku);
            if (editingSku() === sku) {
                handleCancelEditSku();
            }
        }
    };

    return (
        <div class={props.isWindowed ? "w-full h-full bg-slate-900/50 flex flex-col p-6 md:p-10 gap-6 overflow-y-auto custom-scrollbar" : "flex flex-col gap-6"}>
            <div class="flex items-center gap-3 text-cyan-400">
                <Tag size={24} />
                <h2 class="text-ui-label font-black uppercase tracking-[0.2em]">SKU Catalog</h2>
            </div>
            
            <div class="bg-black/60 border border-white/10 rounded-2xl p-6 flex flex-col gap-6 shadow-xl">
                <div class="text-white/60 text-readable">
                    Map stock item SKUs (e.g. <code>plywood_3/4</code>, <code>m3_bolt</code>) to physical descriptions, prices, and suppliers. These catalog records are saved persistently.
                </div>

                <div class="flex flex-col gap-4 max-h-[300px] overflow-y-auto pr-2 custom-scrollbar">
                    <For each={Object.entries(skuCatalogSignal())}>
                        {([sku, meta]) => (
                            <div class="flex items-center justify-between gap-4 p-3 bg-white/5 border border-white/5 rounded-xl hover:border-cyan-500/30 transition-all">
                                <div class="flex flex-col min-w-0 flex-1 gap-1">
                                    <div class="flex items-center gap-2">
                                        <span class="font-bold text-white/95 uppercase tracking-wider text-sm font-mono">{sku}</span>
                                        <Show when={meta.price}>
                                            <span class="text-xs bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 px-2 py-0.5 rounded-full font-mono font-bold">${meta.price}</span>
                                        </Show>
                                    </div>
                                    <div class="text-xs text-white/50 font-sans truncate">
                                        {meta.description || 'No description'}
                                        <Show when={meta.supplier}>
                                            <span class="text-white/30 ml-2 font-mono">({meta.supplier})</span>
                                        </Show>
                                    </div>
                                </div>
                                <div class="flex items-center gap-2">
                                    <button
                                        onClick={() => handleStartEditSku(sku, meta)}
                                        class="p-2 rounded-lg bg-white/5 text-white/70 hover:bg-cyan-500/20 hover:text-cyan-400 transition-all"
                                        title="Edit SKU"
                                    >
                                        <Edit size={16} />
                                    </button>
                                    <button
                                        onClick={() => handleRemoveSku(sku)}
                                        class="p-2 rounded-lg bg-white/5 text-white/50 hover:bg-red-500/20 hover:text-red-400 transition-all"
                                        title="Delete SKU"
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
                        {editingSku() ? `Edit SKU: ${editingSku()}` : 'Add New SKU Record'}
                    </div>
                    <div class="flex flex-col gap-3">
                        <div class="flex flex-col md:flex-row gap-3">
                            <input 
                                type="text"
                                placeholder="SKU (e.g. plywood_3/4)"
                                value={formSku()}
                                onInput={(e) => setFormSku(e.target.value)}
                                disabled={!!editingSku()}
                                class="flex-1 px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all font-mono"
                            />
                            <input 
                                type="text"
                                placeholder="Price (e.g. 45.00)"
                                value={formPrice()}
                                onInput={(e) => setFormPrice(e.target.value)}
                                class="flex-1 md:w-[150px] px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all font-mono"
                            />
                            <input 
                                type="text"
                                placeholder="Supplier (e.g. McMaster)"
                                value={formSupplier()}
                                onInput={(e) => setFormSupplier(e.target.value)}
                                class="flex-1 px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all font-mono"
                            />
                        </div>
                        <div class="flex flex-col md:flex-row gap-3">
                            <input 
                                type="text"
                                placeholder="Item Description (e.g. 3/4 inch Plywood)"
                                value={formDesc()}
                                onInput={(e) => setFormDesc(e.target.value)}
                                class="flex-1 px-4 py-3 rounded-xl bg-slate-950 border border-white/10 text-white/95 placeholder:text-white/20 focus:outline-none focus:border-cyan-400/50 transition-all"
                            />
                            <div class="flex gap-2">
                                <button
                                    onClick={handleSubmitSku}
                                    class="flex-1 md:flex-none px-6 py-3 rounded-xl bg-cyan-500 text-slate-950 font-black uppercase tracking-widest hover:bg-cyan-400 hover:shadow-[0_0_20px_rgba(34,211,238,0.4)] active:scale-95 transition-all flex items-center justify-center gap-2"
                                >
                                    <Plus size={18} />
                                    {editingSku() ? 'Save' : 'Add'}
                                </button>
                                <Show when={editingSku()}>
                                    <button
                                        onClick={handleCancelEditSku}
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
        </div>
    );
};
