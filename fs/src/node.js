import { VFSClosedError } from './vfs_core.js';

/**
 * Socket Helpers (DSL)
 */
export const In = (path, parameters = {}) => ({
  mode: 'read',
  path,
  parameters,
});
export const Out = (path, parameters = {}) => ({
  mode: 'write',
  path,
  parameters,
});
export const Watch = (path, parameters = {}) => ({
  mode: 'watch',
  path,
  parameters,
});

/**
 * Node abstraction for agent-style processors.
 * Refactored for Mesh-VFS: Uses registerProvider instead of watch.
 */
export class Node {
  constructor(vfs, { sockets = {}, execute }) {
    this.vfs = vfs;
    this.socketsDef = sockets;
    this.execute = execute;
    this._fulfilled = new Map(); // path -> data
  }

  stop() {
    // registerProvider is active until VFS closes or overwritten
  }

  async start() {
    const outputs = Object.entries(this.socketsDef).filter(
      ([name, def]) => def.mode === 'write'
    );

    for (const [outName, outDef] of outputs) {
      this.vfs.registerProvider(outDef.path, async (v, selector, context) => {
        try {
          const sockets = {};
          // Internal helpers prefixed with _ to avoid collision with user sockets
          sockets._params = async () => selector.parameters;

          for (const [name, def] of Object.entries(this.socketsDef)) {
            sockets[name] = {};
            if (def.mode === 'read') {
              sockets[name].read = async (overrideParams = {}) => {
                return v.read(
                  def.path,
                  {
                    ...def.parameters,
                    ...selector.parameters,
                    ...overrideParams,
                  },
                  context
                );
              };
              sockets[name].readData = async (overrideParams = {}) => {
                return v.readData(
                  def.path,
                  {
                    ...def.parameters,
                    ...selector.parameters,
                    ...overrideParams,
                  },
                  context
                );
              };
              sockets[name].readText = async (overrideParams = {}) => {
                return v.readText(
                  def.path,
                  {
                    ...def.parameters,
                    ...selector.parameters,
                    ...overrideParams,
                  },
                  context
                );
              };
            } else if (def.mode === 'write') {
              sockets[name].write = async (stream) => {
                this._fulfilled.set(outDef.path, stream);
              };
              sockets[name].writeData = async (data) => {
                this._fulfilled.set(outDef.path, data);
              };
            }
          }

          await this.execute(sockets);

          const result = this._fulfilled.get(outDef.path);
          this._fulfilled.delete(outDef.path);
          return result;
        } catch (err) {
          console.error(
            `[Node ${v.id}] Execution error for ${selector.path}:`,
            err
          );
          return null;
        }
      });
    }
  }
}
