import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';

export default defineConfig({
  plugins: [solidPlugin()],
  server: {
    port: 3030,
    strictPort: true,
  },
  build: {
    target: 'esnext',
  },
  resolve: {
    alias: {
      'react/jsx-runtime': 'solid-js/h/jsx-runtime',
      'react/jsx-dev-runtime': 'solid-js/h/jsx-runtime',
    },
  },
});
