import { JotEvaluator } from './evaluator.js';
import { UserFulfillerManager } from './fulfiller.js';
import { UGCStorage } from './storage.js';

export { JotEvaluator, UserFulfillerManager, UGCStorage };

export class UGCEngine {
  constructor(vfs, mesh = null, storagePath = null) {
    this.vfs = vfs;
    this.mesh = mesh;
    this.evaluator = new JotEvaluator(vfs, mesh);
    this.storage = new UGCStorage(vfs, storagePath);
    this.fulfiller = new UserFulfillerManager(vfs, mesh, this.evaluator, this.storage);
  }

  /**
   * Initializes the UGC engine, loading all persisted user fulfillers
   */
  async init() {
    const persisted = await this.storage.readAll();
    console.log(`[UGCEngine] Restoring ${Object.keys(persisted).length} user-defined operators from registry...`);
    for (const [name, data] of Object.entries(persisted)) {
      try {
        await this.fulfiller.registerFulfiller(name, data.schema, data.script, false);
      } catch (err) {
        console.error(`[UGCEngine] Error restoring fulfiller '${name}':`, err);
      }
    }
  }

  /**
   * Compiles and evaluates a Jot script string
   * @param {string} script 
   * @param {Object} boundVars 
   * @param {Object} schema 
   * @returns {Promise<Array>} Terminals
   */
  async evaluate(script, boundVars = {}, schema = {}) {
    return this.evaluator.evaluate(script, boundVars, schema);
  }

  /**
   * Registers a user-defined operator, wraps as VFS provider, and publishes to Zenoh mesh
   * @param {Object} config
   * @param {string} config.name
   * @param {Object} config.schema
   * @param {string} config.script
   * @param {boolean} [persist=true]
   * @returns {Promise<string>} Registered operator path
   */
  async registerFulfiller({ name, schema, script, persist = true }) {
    return this.fulfiller.registerFulfiller(name, schema, script, persist);
  }

  /**
   * Lists all active user fulfillers
   * @returns {Array} List of operators
   */
  listFulfillers() {
    return this.fulfiller.listFulfillers();
  }
}
