import { createSignal, createEffect, onMount, onCleanup } from 'solid-js';
import { vfs, blackboard } from '../lib/blackboard';

export const DynamicUX = (props) => {
    let containerRef;
    const [error, setError] = createSignal(null);
    const [loaded, setLoaded] = createSignal(false);

    const [status, setStatus] = createSignal('initializing');

    createEffect(async () => {
        const schema = props.schema;
        if (!schema || !schema.ux || !schema.componentName) {
            setStatus('no-ux-config');
            return;
        }

        try {
            setStatus('fetching-script');
            const path = schema.ux.replace('vfs:/', '');
            console.log(`[DynamicUX] Fetching UI script from: ${path}`);
            
            const jsText = await vfs.readData(path, {});
            if (!jsText || typeof jsText !== 'string') {
                console.error('[DynamicUX] Script content is empty or invalid:', jsText);
                throw new Error(`Empty script at ${path}`);
            }

            console.log(`[DynamicUX] Received script (${jsText.length} chars). First 50: "${jsText.slice(0, 50)}..."`);

            setStatus('registering-component');
            const blob = new Blob([jsText], { type: 'text/javascript' });
            const url = URL.createObjectURL(blob);
            
            await import(/* @vite-ignore */ url);
            URL.revokeObjectURL(url);
            
            console.log(`[DynamicUX] Component registered: ${schema.componentName}`);
            setLoaded(true);
            setStatus('ready');
        } catch (e) {
            console.error('[DynamicUX] Error:', e);
            setError(e.message);
            setStatus('error');
        }
    });

    createEffect(() => {
        if (loaded() && containerRef && props.schema.componentName) {
            containerRef.innerHTML = '';
            const el = document.createElement(props.schema.componentName);
            
            if (props.parameters) {
                for (const [key, value] of Object.entries(props.parameters)) {
                    el.setAttribute(key, value);
                }
            }

            const handleParamChange = (e) => {
                if (e.detail) {
                    blackboard.tickle(props.path, e.detail);
                }
            };
            
            el.addEventListener('param-change', handleParamChange);
            containerRef.appendChild(el);

            onCleanup(() => el.removeEventListener('param-change', handleParamChange));
        }
    });

    return (
        <div class="w-full mt-3 border-t border-white/10 pt-3">
            <div class="flex items-center justify-between mb-2">
                <div class="text-[9px] font-black tracking-widest text-capability/80 uppercase flex items-center gap-1">
                    <span>Agent Custom UI</span>
                </div>
                <div class={`text-[7px] font-mono px-1.5 py-0.5 rounded border ${
                    status() === 'ready' ? 'bg-green-500/20 border-green-500/40 text-green-400' :
                    status() === 'error' ? 'bg-red-500/20 border-red-500/40 text-red-400' :
                    'bg-white/5 border-white/10 text-white/40'
                }`}>
                    {status().toUpperCase()}
                </div>
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
