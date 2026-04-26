import { Show, createSignal } from 'solid-js';
import { blackboard } from '../lib/blackboard';
import { AlertTriangle, RefreshCw, X } from 'lucide-solid';

export const ErrorOverlay = () => {
  const error = blackboard.error;
  const setError = blackboard.setError;
  const [showStack, setShowStack] = createSignal(false);

  const handleReload = () => {
    window.location.reload();
  };

  const handleDismiss = () => {
    setError(null);
    setShowStack(false);
  };

  return (
    <Show when={error()}>
      <div class="fixed inset-0 z-[9999] flex items-center justify-center bg-black/80 backdrop-blur-sm p-4">
        <div class="w-full max-w-2xl bg-slate-900 border-2 border-red-500 rounded-lg shadow-2xl shadow-red-500/20 overflow-hidden flex flex-col max-h-[90vh]">
          {/* Header */}
          <div class="bg-red-500 p-4 flex items-center justify-between text-white">
            <div class="flex items-center gap-3 font-bold text-lg">
              <AlertTriangle size={24} />
              <span>SYSTEM EXCEPTION</span>
            </div>
            <button 
              onClick={handleDismiss}
              class="hover:bg-red-600 p-1 rounded transition-colors"
              title="Dismiss"
            >
              <X size={20} />
            </button>
          </div>

          {/* Body */}
          <div class="p-6 flex-1 overflow-y-auto space-y-4">
            <div class="text-red-400 font-mono text-lg break-words">
              {error()?.message || 'An unknown error occurred'}
            </div>

            <Show when={error()?.stack}>
              <div class="space-y-2 text-slate-300">
                <button 
                  onClick={() => setShowStack(!showStack())}
                  class="text-xs uppercase tracking-widest font-bold text-slate-500 hover:text-slate-300 transition-colors flex items-center gap-1"
                >
                  {showStack() ? 'Hide' : 'Show'} Technical Details
                </button>
                
                <Show when={showStack()}>
                  <pre class="bg-black/50 p-4 rounded border border-slate-800 font-mono text-xs overflow-x-auto whitespace-pre-wrap leading-relaxed text-slate-400">
                    {error().stack}
                  </pre>
                </Show>
              </div>
            </Show>
          </div>

          {/* Footer */}
          <div class="p-4 bg-slate-800/50 border-t border-slate-800 flex justify-end gap-3">
            <button 
              onClick={handleDismiss}
              class="px-4 py-2 rounded text-slate-400 hover:text-white hover:bg-slate-700 transition-all font-medium text-sm"
            >
              DISMISS
            </button>
            <button 
              onClick={handleReload}
              class="px-4 py-2 bg-red-500/10 border border-red-500 text-red-500 hover:bg-red-500 hover:text-white transition-all font-bold text-sm flex items-center gap-2 rounded"
            >
              <RefreshCw size={16} />
              HARD RELOAD
            </button>
          </div>
        </div>
      </div>
    </Show>
  );
};
