import { createSignal, createEffect, onMount, onCleanup } from 'solid-js';
import { vfs, blackboard } from '../lib/blackboard';

export const DynamicUX = (props) => {
    let containerRef;
    const [error, setError] = createSignal(null);
    const [loaded, setLoaded] = createSignal(false);

    createEffect(async () => {
        const schema = props.schema;
        if (!schema || !schema.ux || !schema.componentName) return;

        try {
            // 1. Fetch the JS text from VFS
            const path = schema.ux.replace('vfs:/', '');
            const jsText = await vfs.readData(path, {});
            
            if (!jsText || typeof jsText !== 'string') {
                throw new Error('Could not read valid UX script from VFS');
            }

            // 2. Create an executable Blob URL
            const blob = new Blob([jsText], { type: 'text/javascript' });
            const url = URL.createObjectURL(blob);
            
            // 3. Dynamically import it to evaluate and register customElements
            await import(url);
            URL.revokeObjectURL(url); // Clean up
            
            setLoaded(true);
        } catch (e) {
            console.error('[DynamicUX] Failed to load component:', e);
            setError(e.message);
        }
    });

    createEffect(() => {
        if (loaded() && containerRef && props.schema.componentName) {
            // Clear existing
            containerRef.innerHTML = '';
            
            // 4. Render the Web Component
            const el = document.createElement(props.schema.componentName);
            
            // Pass parameters as attributes
            if (props.parameters) {
                for (const [key, value] of Object.entries(props.parameters)) {
                    el.setAttribute(key, value);
                }
            }

            // 5. Listen for events from the custom element
            const handleParamChange = (e) => {
                if (e.detail) {
                    console.log(`[DynamicUX] ${props.schema.componentName} requested param change:`, e.detail);
                    blackboard.tickle(props.path, e.detail);
                }
            };
            
            el.addEventListener('param-change', handleParamChange);
            containerRef.appendChild(el);

            onCleanup(() => {
                el.removeEventListener('param-change', handleParamChange);
            });
        }
    });

    return (
        <div class="w-full mt-3 border-t border-white/10 pt-3">
            <div class="text-[9px] font-black tracking-widest text-capability/80 uppercase mb-2 flex items-center gap-1">
                <span>Agent Custom UI</span>
            </div>
            {error() ? (
                <div class="text-red-400 text-xs p-2 bg-red-900/20 rounded border border-red-500/20">
                    Failed to load UI: {error()}
                </div>
            ) : (
                <div ref={containerRef} class="min-h-12 flex items-center justify-center text-white/30 text-[10px] italic">
                    {!loaded() ? "Loading agent UI..." : ""}
                </div>
            )}
        </div>
    );
};
