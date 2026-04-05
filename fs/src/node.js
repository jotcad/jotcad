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
 * Composes a final selector.
 */
const composeSelector = (definition, triggerParams, callParams) => {
  return {
    path: definition.path,
    mode: definition.mode,
    parameters: {
      ...definition.parameters,
      ...triggerParams,
      ...callParams,
    },
  };
};

/**
 * Node abstraction for agent-style processors using Socket Selectors.
 */
export class Node {
  constructor(vfs, { sockets = {}, execute }) {
    this.vfs = vfs;
    this.socketsDef = sockets;
    this.execute = execute;
    this.abortController = new AbortController();
  }

  stop() {
    console.log('[Node] Stopping agent...');
    this.abortController.abort();
  }

  async start() {
    const triggers = Object.entries(this.socketsDef).filter(
      ([name, def]) => def.mode === 'write'
    );

    const promises = triggers.map(([outName, outDef]) => {
      return (async () => {
        try {
          const query = this.vfs.watch(outDef.path, {
            states: ['PENDING'],
            signal: this.abortController.signal,
          });

          for await (const req of query) {
            if (this.abortController.signal.aborted) break;

            const hasLease = await this.vfs.lease(
              req.path,
              req.parameters,
              30000
            );
            if (!hasLease) continue;

            try {
              const sockets = {};
              sockets.params = async () => req.parameters;

              for (const [name, def] of Object.entries(this.socketsDef)) {
                sockets[name] = {};
                if (def.mode === 'read') {
                  sockets[name].read = async (overrideParams = {}) => {
                    return this.vfs.read(def.path, {
                      ...def.parameters,
                      ...req.parameters,
                      ...overrideParams,
                    });
                  };
                } else if (def.mode === 'write') {
                  sockets[name].write = async (stream, overrideParams = {}) => {
                    return this.vfs.write(
                      def.path,
                      {
                        ...def.parameters,
                        ...req.parameters,
                        ...overrideParams,
                      },
                      stream
                    );
                  };
                } else if (def.mode === 'watch') {
                  sockets[name].watch = (options = {}) => {
                    return this.vfs.watch(def.path, {
                      ...options,
                      signal: this.abortController.signal,
                    });
                  };
                }
              }

              await this.execute(sockets);
            } catch (err) {
              if (
                err instanceof VFSClosedError ||
                this.abortController.signal.aborted
              )
                break;
              throw err;
            }
          }
        } catch (err) {
          if (err instanceof VFSClosedError || err.name === 'AbortError')
            return;
          throw err;
        }
      })();
    });

    return Promise.all(promises);
  }
}
