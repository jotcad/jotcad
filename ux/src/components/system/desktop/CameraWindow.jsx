import { createSignal, onMount, onCleanup, Show } from 'solid-js';
import { vfs, mesh, Selector } from '../../../lib/VFSManager';

export const CameraWindow = (props) => {
  const [rawImage, setRawImage] = createSignal(null);
  const [preprocessedImage, setPreprocessedImage] = createSignal(null);

  const decodeGrayscale = (payload, width, height) => {
    if (!payload) return null;
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext('2d');
    const imageData = ctx.createImageData(width, height);
    for (let i = 0; i < payload.length; i++) {
        const val = payload[i];
        const j = i * 4;
        imageData.data[j] = val;     // R
        imageData.data[j + 1] = val; // G
        imageData.data[j + 2] = val; // B
        imageData.data[j + 3] = 255; // A
    }
    ctx.putImageData(imageData, 0, 0);
    return canvas.toDataURL('image/png');
  };

  onMount(() => {
    const handleNotify = (selector, payload) => {
      if (selector.path === 'sensor/camera') {
        const width = selector.parameters?.width || 96;
        const height = selector.parameters?.height || 96;
        const url = decodeGrayscale(payload, width, height);
        if (url) {
          const prev = rawImage();
          if (prev && prev.startsWith('data:')) {
            // No URL.revokeObjectURL needed for data URLs, but good to clean up
          }
          setRawImage(url);
        }
      } else if (selector.path === 'sensor/camera/preprocessed') {
        const width = selector.parameters?.width || 28;
        const height = selector.parameters?.height || 28;
        const url = decodeGrayscale(payload, width, height);
        if (url) {
          const prev = preprocessedImage();
          if (prev && prev.startsWith('data:')) {
            // Clean up
          }
          setPreprocessedImage(url);
        }
      }
    };

    vfs.events.on('notify', handleNotify);

    // Subscribe to both topics
    const rawSel = new Selector('sensor/camera', { width: 96, height: 96, format: 'grayscale' });
    const prepSel = new Selector('sensor/camera/preprocessed', { width: 28, height: 28, format: 'grayscale' });

    mesh.subscribe(rawSel, Date.now() + 3600000).catch(() => {});
    mesh.subscribe(prepSel, Date.now() + 3600000).catch(() => {});

    onCleanup(() => {
      vfs.events.off('notify', handleNotify);
    });
  });

  return (
    <div class="w-full h-full p-4 flex flex-col bg-black/40 text-white select-none box-border">
      <div class="grid grid-cols-2 gap-4 flex-1 min-h-0">
        <div class="flex flex-col items-center gap-2 bg-white/5 rounded-lg p-2 border border-white/10 min-h-0">
          <span class="text-[10px] font-black tracking-wider text-cyan-400">RAW VIEWPORT (96x96)</span>
          <div class="flex-1 w-full flex items-center justify-center min-h-0 overflow-hidden">
            <Show when={rawImage()} fallback={<div class="text-white/20 animate-pulse text-xs">Waiting for feed...</div>}>
              <img 
                src={rawImage()} 
                style={{ 'image-rendering': 'pixelated' }}
                class="w-full h-full object-contain border border-cyan-500/30" 
              />
            </Show>
          </div>
        </div>
        <div class="flex flex-col items-center gap-2 bg-white/5 rounded-lg p-2 border border-white/10 min-h-0">
          <span class="text-[10px] font-black tracking-wider text-emerald-400">PREPROCESSED (48x48)</span>
          <div class="flex-1 w-full flex items-center justify-center min-h-0 overflow-hidden">
            <Show when={preprocessedImage()} fallback={<div class="text-white/20 animate-pulse text-xs">Waiting for feed...</div>}>
              <img 
                src={preprocessedImage()} 
                style={{ 'image-rendering': 'pixelated' }}
                class="w-full h-full object-contain border border-emerald-500/30" 
              />
            </Show>
          </div>
        </div>
      </div>
    </div>
  );
};
