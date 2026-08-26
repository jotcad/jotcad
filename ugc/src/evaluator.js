import { JotParser } from '../../jot/src/parser.js';
import { JotCompiler } from '../../jot/src/compiler.js';

export class JotEvaluator {
  constructor(vfs, mesh = null) {
    this.vfs = vfs;
    this.mesh = mesh;
    this.parser = new JotParser();
    this.compiler = new JotCompiler(vfs);
  }

  /**
   * Sync operator definitions from the local VFS providers and remote mesh catalog
   */
  syncOperators() {
    // 1. Sync local VFS provider schemas
    if (this.vfs && this.vfs.schemas) {
      for (const [path, schema] of this.vfs.schemas.entries()) {
        this.compiler.registerOperator(path, { path, schema });
      }
    }

    // 2. Sync remote mesh operators
    if (this.mesh && this.mesh.catalog) {
      for (const [path, schema] of Object.entries(this.mesh.catalog)) {
        this.compiler.registerOperator(path, { path, schema });
      }
    }
  }

  /**
   * Register an operator schema into the compiler environment
   * @param {string} name 
   * @param {Object} opConfig 
   */
  registerOperator(name, { path, schema }) {
    this.compiler.registerOperator(name, { path, schema });
  }

  /**
   * Parse a Jot script string to AST
   * @param {string} script 
   * @returns {Object} AST
   */
  parse(script) {
    return this.parser.parse(script.trim());
  }

  /**
   * Evaluates a Jot script string with bound variables and output schemas
   * @param {string} script 
   * @param {Object} boundVars 
   * @param {Object} schema 
   * @param {string} qualifiedName 
   * @returns {Promise<Array>} Evaluation terminals
   */
  async evaluate(script, boundVars = {}, schema = {}, qualifiedName = 'user/Expression') {
    this.syncOperators();
    const ast = this.parse(script);
    
    const outputs = schema.outputs || { $out: { type: 'jot:shape' } };
    const normalizedOutputs = {};
    for (const [port, def] of Object.entries(outputs)) {
      let type = def.type || 'jot:shape';
      if (!type.includes(':')) type = 'jot:' + type;
      normalizedOutputs[port] = { ...def, type };
    }

    const testSchema = {
      arguments: schema.arguments || [],
      outputs: normalizedOutputs
    };

    return this.compiler.evaluate(ast, boundVars, testSchema, qualifiedName);
  }
}
