import { Selector } from '../../fs/src/index.js';

export class UserFulfillerManager {
  constructor(vfs, mesh, evaluator, storage) {
    this.vfs = vfs;
    this.mesh = mesh;
    this.evaluator = evaluator;
    this.storage = storage;
    this.activeFulfillers = new Map(); // name -> { schema, script }
  }

  /**
   * Registers a user-defined operator fulfiller
   * @param {string} name e.g., 'user/CustomBox' or 'user/CustomBox:v1'
   * @param {Object} schema The operator arguments and outputs schema
   * @param {string} script The Jot script execution source
   * @param {boolean} persist Whether to persist this operator to disk
   */
  async registerFulfiller(name, schema, script, persist = true) {
    let path = name;
    if (!path.startsWith('jot/') && !path.startsWith('user/')) {
      path = `user/${path}`;
    }

    if (!schema || !schema.arguments) {
      throw new Error(`UGC Register Error: Operator '${path}' must have a valid schema with arguments defined.`);
    }

    // 1. Register VFS Provider Callback
    this.vfs.registerProvider(path, async (vfsNode, selector, context) => {
      const boundVars = {};
      for (const arg of schema.arguments || []) {
        boundVars[arg.name] = selector.parameters[arg.name] ?? arg.default;
      }

      // Evaluate script using JotEvaluator
      const results = await this.evaluator.evaluate(script, boundVars, schema, path);
      
      const portName = selector.output || '$out';
      const outTerm = results.find(t => t.port === portName);
      if (!outTerm) {
        throw new Error(`UGC Fulfiller Error: Fulfiller '${path}' failed to produce output port '${portName}'`);
      }

      return vfsNode.readSelector(outTerm.selector);
    }, schema.arguments.map(arg => arg.name));

    // 2. Register within Compiler (compiler handles short-name extraction automatically)
    this.evaluator.registerOperator(path, { path, schema });

    // 3. Register Zenoh Mesh Queryable & update catalog
    if (this.mesh && typeof this.mesh.registerOp === 'function') {
      await this.mesh.registerOp(path);
      if (typeof this.mesh.publishCatalogUpdate === 'function') {
        await this.mesh.publishCatalogUpdate();
      }
    }

    this.activeFulfillers.set(path, { schema, script });

    // 4. Save to persistent storage
    if (persist && this.storage) {
      await this.storage.save(path, schema, script);
    }

    return path;
  }

  /**
   * Lists all currently active user fulfillers
   * @returns {Array} List of active user operators
   */
  listFulfillers() {
    return Array.from(this.activeFulfillers.entries()).map(([path, data]) => ({
      path,
      schema: data.schema,
      script: data.script
    }));
  }
}
