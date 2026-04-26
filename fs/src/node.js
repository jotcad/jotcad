import { VFSClosedError } from './vfs_core.js';
import { Selector } from './cid.js';

/**
 * Socket Helpers (DSL)
 */
export const In = (path, parameters = {}, port) => ({
  mode: 'read',
  selector: port ? new Selector(path, parameters).withOutput(port) : new Selector(path, parameters)
});
export const Out = (path, parameters = {}, port) => ({
  mode: 'write',
  selector: port ? new Selector(path, parameters).withOutput(port) : new Selector(path, parameters)
});
export const Watch = (path, parameters = {}) => ({
  mode: 'watch',
  selector: new Selector(path, parameters)
});

/**
 * Node: An atomic computational unit that reacts to mesh events.
 */
export class LegacyNode {
  constructor({ id, vfs, sockets = {}, execute = null }) {
    this.id = id || `node-${Math.random().toString(36).slice(2, 6)}`;
    this.vfs = vfs;
    this.socketsDef = sockets;
    this._fulfilled = new Map();
    if (execute) this.execute = execute;
  }

  async execute(sockets) {
    throw new Error('Node.execute: Not implemented');
  }

  start() {
    const v = this.vfs;
    for (const [outName, outDef] of Object.entries(this.socketsDef)) {
      if (outDef.mode !== 'write') continue;

      const baseSelector = outDef.selector;

      v.registerProvider(baseSelector.path, async (vfs, selector, context) => {
        try {
          const sockets = {};
          // Internal helpers prefixed with _ to avoid collision with user sockets
          sockets._params = async () => selector.parameters;

          for (const [name, def] of Object.entries(this.socketsDef)) {
            sockets[name] = {};
            if (def.mode === 'read') {
              sockets[name].read = async (overrideParams = {}) => {
                const s = new Selector(
                  def.selector.path,
                  { ...def.selector.parameters, ...selector.parameters, ...overrideParams },
                  def.selector.output
                );
                return v.read(s, context);
              };
              sockets[name].readData = async (overrideParams = {}) => {
                const s = new Selector(
                  def.selector.path,
                  { ...def.selector.parameters, ...selector.parameters, ...overrideParams },
                  def.selector.output
                );
                return v.readData(s, context);
              };
              sockets[name].readText = async (overrideParams = {}) => {
                const s = new Selector(
                  def.selector.path,
                  { ...def.selector.parameters, ...selector.parameters, ...overrideParams },
                  def.selector.output
                );
                return v.readText(s, context);
              };
            } else if (def.mode === 'write') {
              sockets[name].write = async (stream) => {
                const s = new Selector(
                    def.selector.path,
                    { ...def.selector.parameters, ...selector.parameters },
                    def.selector.output
                );
                this._fulfilled.set(def.selector.path, stream);
                return v.write(s, stream);
              };
              sockets[name].writeData = async (data) => {
                const s = new Selector(
                    def.selector.path,
                    { ...def.selector.parameters, ...selector.parameters },
                    def.selector.output
                );
                this._fulfilled.set(def.selector.path, data);
                return v.writeData(s, data);
              };
            }
          }

          await this.execute(sockets);

          const result = this._fulfilled.get(baseSelector.path);
          this._fulfilled.delete(baseSelector.path);
          return result;
        } catch (err) {
          console.error(
            `[Node ${v.id}] Execution error for ${selector.path}:`,
            err
          );
          throw err; // Fail loudly
        }
      });
    }
  }
}
