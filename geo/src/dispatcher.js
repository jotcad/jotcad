import { spawn } from 'node:child_process';
import path from 'node:path';

/**
 * A Dispatcher watches the VFS for demand and spawns ephemeral C++ agents.
 */
export class Dispatcher {
  constructor(vfs, { hubUrl, binDir }) {
    this.vfs = vfs;
    this.hubUrl = hubUrl;
    this.binDir = binDir;
    this.agents = new Map(); // path prefix -> binary name
  }

  register(pathPrefix, binaryName) {
    this.agents.set(pathPrefix, binaryName);
  }

  async start() {
    // Watch for ANY pending request in the 'shape/' space
    const query = this.vfs.watch('shape/*', { states: ['PENDING'] });
    
    for await (const req of query) {
      // Find the best matching agent
      const binary = this._findAgent(req.path);
      if (!binary) continue;

      const binaryPath = path.join(this.binDir, binary);
      
      console.log(`[Dispatcher] Spawning ${binary} for ${req.path}`);
      
      // Spawn the single-task agent
      const child = spawn(binaryPath, [
        '--hub', this.hubUrl,
        '--path', req.path,
        '--params', JSON.stringify(req.parameters),
        '--peer', `dispatcher-spawned-${Math.random().toString(36).slice(2)}`
      ]);

      child.stdout.on('data', (data) => console.log(`[${binary} stdout] ${data}`));
      child.stderr.on('data', (data) => console.error(`[${binary} stderr] ${data}`));
      
      child.on('close', (code) => {
        if (code !== 0) {
          console.error(`[Dispatcher] Agent ${binary} exited with code ${code}`);
        }
      });
    }
  }

  _findAgent(vfsPath) {
    for (const [prefix, binary] of this.agents.entries()) {
      if (vfsPath.startsWith(prefix)) return binary;
    }
    return null;
  }
}
