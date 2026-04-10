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
    this.schemas = new Map(); // path prefix -> JSON schema
    this.abortController = new AbortController();
  }

  register(pathPrefix, binaryName) {
    this.agents.set(pathPrefix, binaryName);
  }

  declareSchema(pathPrefix, schema) {
    this.schemas.set(pathPrefix, schema);
  }

  stop() {
    this.abortController.abort();
  }

  async start() {
    // Announce registered agents as LISTENING states
    for (const [prefix, binary] of this.agents.entries()) {
      const cid = await this.vfs.getCID({ path: prefix, parameters: {} });
      await this.vfs.receive({
        cid,
        path: prefix,
        parameters: {},
        state: 'LISTENING',
        source: `agent:${binary}`
      });
    }

    // Announce registered schemas
    for (const [prefix, schema] of this.schemas.entries()) {
      await this.vfs.declare(prefix, schema);
    }

    try {
      const query = this.vfs.watch('*', { 
        states: ['PENDING'],
        signal: this.abortController.signal
      });
      
      for await (const req of query) {
        if (this.abortController.signal.aborted) break;

        const binary = this._findAgent(req.path);
        if (!binary) continue;

        const binaryPath = path.join(this.binDir, binary);
        console.log(`[Dispatcher] Spawning ${binary} for ${req.path}`);
        
        const child = spawn(binaryPath, [
          '--hub', this.hubUrl,
          '--path', req.path,
          '--params', JSON.stringify(req.parameters),
          '--peer', `dispatcher-spawned-${Math.random().toString(36).slice(2)}`
        ]);

        child.stdout.on('data', (data) => console.log(`[${binary} stdout] ${data}`));
        child.stderr.on('data', (data) => console.error(`[${binary} stderr] ${data}`));
        
        child.on('close', (code) => {
          if (code !== 0 && !this.abortController.signal.aborted) {
            console.error(`[Dispatcher] Agent ${binary} exited with code ${code}`);
          }
        });
      }
    } catch (err) {
      if (err.name === 'AbortError' || err.message.includes('closed')) return;
      throw err;
    }
  }

  _findAgent(vfsPath) {
    for (const [prefix, binary] of this.agents.entries()) {
      if (vfsPath.startsWith(prefix)) return binary;
    }
    return null;
  }
}
