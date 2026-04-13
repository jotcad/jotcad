import { defineConfig } from 'vitest/config';
import solidPlugin from 'vite-plugin-solid';

export default defineConfig({
  plugins: [solidPlugin()],
  test: {
    environment: 'jsdom',
    globals: true,
    // This is needed for Solid.js to work correctly in Vitest
    resolveSnapshotPath: (testPath, snapshotExtension) => testPath + snapshotExtension,
    server: {
      deps: {
        inline: [/solid-js/],
      },
    },
  },
  resolve: {
    conditions: ['development', 'browser'],
  },
});
