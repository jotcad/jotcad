import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import fs from 'node:fs';
import path from 'node:path';

const sslDir = path.resolve(__dirname, '../.ssl');
const keyPath = path.join(sslDir, 'localhost-key.pem');
const certPath = path.join(sslDir, 'localhost-cert.pem');

const https = (process.env.VITE_HTTPS !== 'false') && fs.existsSync(keyPath) && fs.existsSync(certPath) ? {
  key: fs.readFileSync(keyPath),
  cert: fs.readFileSync(certPath),
} : false;

export default defineConfig({
  plugins: [solidPlugin()],
  optimizeDeps: {
    include: ['node-diff3', 'trimerge', 'lucide-solid'],
  },
  server: {
    port: 3030,
    strictPort: true,
    host: true,
    https,
    hmr: false
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
