import { normalizeSelector, Selector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Next-Gen Compiler
 */
export class JotCompiler {
  constructor(vfs = null, options = {}) {
    this.operators = new Map();
    this.vfs = vfs;
    this.options = { optimizeAliases: true, defaultPrefix: 'jot/', ...options };
    this.sideDemands = new Map();
    this.symbolTypes = {};

    this.consumers = {
      'jot:number': (p, a, c, s) => this.JotNumberConsumer(p, a, c, s),
      'jot:numbers': (p, a, c, s) => this.JotNumbersConsumer(p, a, c, s),
      'jot:string': (p, a, c, s) => this.JotStringConsumer(p, a, c, s),
      'jot:strings': (p, a, c, s) => this.JotStringsConsumer(p, a, c, s),
      'jot:boolean': (p, a, c, s) => this.JotBooleanConsumer(p, a, c, s),
      'jot:shape': (p, a, c, s) => this.JotShapeConsumer(p, a, c, s),
      'jot:shapes': (p, a, c, s) => this.JotShapesConsumer(p, a, c, s),
      'jot:flags': (p, a, c, s) => this.JotFlagsConsumer(p, a, c, s),
      'jot:vec3': (p, a, c, s) => this.JotVec3Consumer(p, a, c, s),
      'jot:interval': (p, a, c, s) => this.JotIntervalConsumer(p, a, c, s),
      'jot:operation': (p, a, c, s) => this.JotAnyConsumer(p, a, c, s),
      'jot:op': (p, a, c, s) => this.JotAnyConsumer(p, a, c, s),
      'jot:selector': (p, a, c, s) => this.JotAnyConsumer(p, a, c, s),
      'jot:any': (p, a, c, s) => this.JotAnyConsumer(p, a, c, s),
      'any': (p, a, c, s) => this.JotAnyConsumer(p, a, c, s),
    };
  }

  // --- Type Predicates ---

  _checkSymbol(v, type, argDef, ctx) {
    const symType = this.symbolTypes[v.name];
    if (symType && symType !== type) {
      throw new Error(`Compiler Error: Symbol '${v.name}' (type ${symType}) used in ${type} slot '${argDef.name}' of '${ctx.opName}'`);
    }
    return true;
  }


  _getSelectorType(sel) {
    const schema = this._getSchemaForPath(sel.path);
    return schema?.outputs?.[sel.output || '$out']?.type;
  }

  _isJotNumber(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:number', a, c);
    if (v instanceof Selector) return this._getSelectorType(v) === 'jot:number';
    return typeof v === 'number';
  }

  _isJotString(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:string', a, c);
    if (v instanceof Selector) return this._getSelectorType(v) === 'jot:string';
    return typeof v === 'string';
  }

  _isJotBoolean(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:boolean', a, c);
    if (v instanceof Selector) return this._getSelectorType(v) === 'jot:boolean';
    return typeof v === 'boolean';
  }

  _isJotShape(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:shape', a, c);
    if (v instanceof Selector) return this._getSelectorType(v) === 'jot:shape';
    return typeof v === 'object' && v !== null && v.path;
  }

  _isJotShapeList(v, a, c) {
    return Array.isArray(v) && v.every(item => this._isJotShape(item, a, c));
  }

  _isJotVec3(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:vec3', a, c);
    return Array.isArray(v) && v.length === 3;
  }

  _isJotInterval(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:interval', a, c);
    return typeof v === 'number' || (Array.isArray(v) && (v.length === 1 || v.length === 2));
  }

  // --- Helper Methods ---

  _findCandidates(pool, name) {
    return pool.filter(p => !p.consumed && (p.nameHint === name || !p.nameHint));
  }

  _getSchemaForPath(path) {
    for (const variants of this.operators.values()) {
      const found = variants.find(v => v.path === path);
      if (found) return found.schema;
    }
    return null;
  }

  _parseSignature(typeStr) {
    const match = typeStr.match(/<(.+)>$/);
    if (!match) return null;
    const parts = match[1].split(',').map(s => s.trim());
    const signature = { inputs: {}, output: 'jot:any' };
    for (const part of parts) {
      const pair = part.split(':').map(s => s.trim());
      if (pair.length === 2) {
          const [key, val] = pair;
          const fullVal = val.startsWith('jot:') ? val : 'jot:' + val;
          if (key === '$out') signature.output = fullVal;
          else signature.inputs[key] = fullVal;
      }
    }
    return signature;
  }

  // --- Consumers ---

  async JotNumberConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotNumber(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotNumber(subject, argDef, ctx)) return subject;
    return undefined;
  }

  async JotNumbersConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);

        if (val instanceof Selector) {
            const type = this._getSelectorType(val);
            if (type === 'jot:number' || type === 'jot:numbers') {
                results.push(val);
                p.consumed = true;
                continue;
            }
        }

        if (this._isJotNumber(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else if (Array.isArray(val) && val.every(v => this._isJotNumber(v, argDef, ctx))) {
            results.push(...val);
            p.consumed = true;
        } else break;
    }
    if (results.length === 0 && subject !== null) {
        if (this._isJotNumber(subject, argDef, ctx)) results.push(subject);
        else if (Array.isArray(subject) && subject.every(v => this._isJotNumber(v, argDef, ctx))) results.push(...subject);
    }
    return results.length > 0 ? results : undefined;
  }

  async JotStringConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotString(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotString(subject, argDef, ctx)) return subject;
    return undefined;
  }

  async JotStringsConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotString(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    if (results.length === 0 && subject !== null && this._isJotString(subject, argDef, ctx)) {
        results.push(subject);
    }
    return results.length > 0 ? results : undefined;
  }

  async JotBooleanConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotBoolean(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotBoolean(subject, argDef, ctx)) return subject;
    return undefined;
  }

  async JotShapeConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotShape(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotShape(subject, argDef, ctx)) return subject;
    return undefined;
  }

  async JotShapesConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotShape(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else if (this._isJotShapeList(val, argDef, ctx)) {
            results.push(...val);
            p.consumed = true;
        } else break;
    }
    if (results.length === 0 && subject !== null) {
        if (this._isJotShape(subject, argDef, ctx)) results.push(subject);
        else if (this._isJotShapeList(subject, argDef, ctx)) results.push(...subject);
    }
    return results.length > 0 ? results : undefined;
  }

  async JotFlagsConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const flags = {};
    let found = false;
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (typeof val === 'string' || val?.type === 'SYMBOL') {
            const key = val?.type === 'SYMBOL' ? val.name : val;
            flags[key] = true;
            p.consumed = true;
            found = true;
        } else break;
    }
    return found ? flags : undefined;
  }

  async JotVec3Consumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotVec3(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotVec3(subject, argDef, ctx)) return subject;
    return undefined;
  }

  async JotIntervalConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotInterval(val, argDef, ctx)) {
          p.consumed = true;
          if (typeof val === 'number') return [-val / 2, val / 2];
          if (Array.isArray(val) && val.length === 1) return [0, val[0]];
          return val;
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotInterval(subject, argDef, ctx)) {
        if (typeof subject === 'number') return [-subject / 2, subject / 2];
        if (Array.isArray(subject) && subject.length === 1) return [0, subject[0]];
        return subject;
    }
    return undefined;
  }

  async JotAnyConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    if (candidates.length > 0) {
        const p = candidates[0];
        const val = await ctx.evaluate(p.node);
        p.consumed = true;
        return val;
    }
    return subject !== null ? subject : undefined;
  }

  // --- Core API ---

  registerOperator(name, config) {
    if (!config.schema?.arguments || !Array.isArray(config.schema.arguments)) {
       throw new Error(`Compiler Error: Operator '${name}' must provide an array-based arguments schema.`);
    }

    const register = (n) => {
      const list = this.operators.get(n) || [];
      if (!list.includes(config)) {
        list.push(config);
        this.operators.set(n, list);
      }
    };

    register(name);
    register(config.path);

    // Support Base-Name mapping for variants (e.g. Hexagon/full -> Hexagon)
    if (name.includes('/')) {
      register(name.split('/')[0]);
    }
  }

  async evaluate(ast, parameters = {}, symbolTypes = {}) {
    this.symbolTypes = symbolTypes;
    try {
      if (Array.isArray(ast)) {
        const results = [];
        for (const node of ast) {
          results.push(await this._evaluateRecursive(node, parameters, null));
        }
        return results;
      }
      return await this._evaluateRecursive(ast, parameters, null);
    } finally {
      this.symbolTypes = {};
    }
  }

  async _evaluateRecursive(node, parameters, subject) {
    if (node === null || node === undefined) return node;
    if (typeof node !== 'object') return node;
    if (Array.isArray(node)) {
        const results = [];
        for (const item of node) results.push(await this._evaluateRecursive(item, parameters, subject));
        return results;
    }

    switch (node.type) {
      case 'CALL': return this._dispatchCall(node, parameters, subject);
      case 'METHOD': return this._evaluateMethod(node, parameters, subject);
      case 'SYMBOL': return parameters[node.name] !== undefined ? parameters[node.name] : node;
      case 'ANNOTATED_ARG': return this._evaluateRecursive(node.value, parameters, subject);
      default:
        const res = {};
        for (const [k, v] of Object.entries(node)) res[k] = await this._evaluateRecursive(v, parameters, subject);
        return res;
    }
  }

  async _evaluateMethod(node, parameters, subject) {
    const s = await this._evaluateRecursive(node.subject, parameters, subject);
    
    // Universal Mapping Principle
    if (Array.isArray(s)) {
      const results = [];
      for (const item of s) {
        results.push(await this._dispatchCall(node, parameters, item));
      }
      return results.flat();
    }

    const result = await this._dispatchCall(node, parameters, s);

    // Optimized Aliases (Side Demands)
    if (this.options.optimizeAliases) {
      const candidates = this._resolveOperator(node.name);
      const op = candidates.find(c => c.metadata?.aliases?.[result.output || '$out']);
      if (op) {
        const aliasTarget = op.metadata.aliases[result.output || '$out'];
        const inputKey = (op.schema.arguments || []).find(a => a.name === aliasTarget || (aliasTarget === '$in' && (a.affiliate === '$in' || a.affiliate === '$out')))?.name;
        if (inputKey && result.parameters[inputKey]) {
            this.sideDemands.set(result.toString(), result);
            return result.parameters[inputKey];
        }
      }
    }

    return result;
  }

  async _dispatchCall(node, parameters, subject) {
    const candidates = this._resolveOperator(node.name);
    if (!candidates || candidates.length === 0) throw new Error(`Compiler Error: Unregistered operator '${node.name}'`);

    for (const op of candidates) {
      try {
        const pool = node.args.map(a => ({ 
            node: a, 
            nameHint: a.type === 'ANNOTATED_ARG' ? a.nameHint : null, 
            consumed: false 
        }));
        const params = await this._satisfySchema(op.schema, pool, parameters, subject, node.name);
        const port = op.schema.outputs ? Object.keys(op.schema.outputs)[0] : '$out';
        return new Selector(op.path, params).withOutput(port);
      } catch (e) {
        if (candidates.length === 1) throw e;
        continue;
      }
    }
    throw new Error(`Compiler Error: No variant of '${node.name}' satisfied the provided arguments.`);
  }

  _resolveOperator(name) {
    if (this.operators.has(name)) return this.operators.get(name);
    if (this.operators.has(this.options.defaultPrefix + name)) return this.operators.get(this.options.defaultPrefix + name);
    return null;
  }

  async _satisfySchema(schema, pool, parameters, subject, opName) {
    const params = {}, argList = schema?.arguments || [];

    const evaluateHelper = async (node) => {
        return this._evaluateRecursive(node, parameters, subject);
    };

    // PASS 1: Fill Non-Affiliate slots from the Pool
    for (const argDef of argList) {
      const isAffiliate = argDef.affiliate && (argDef.affiliate === '$out' || argDef.affiliate === '$in');
      if (isAffiliate) continue;

      const type = argDef.type?.toLowerCase() || '';
      const fullType = type.startsWith('jot:') ? type : 'jot:' + type;
      const consumer = this.consumers[fullType.split('<')[0]];
      if (consumer) {
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper }, null);
        if (res !== undefined) {
          if (argDef.const !== undefined && res !== argDef.const) throw new Error("Const mismatch");
          params[argDef.name] = res;
        }
      }

      if (params[argDef.name] === undefined && argDef.default !== undefined) {
        params[argDef.name] = argDef.default;
      }
    }

    // PASS 2: Fill Affiliate slots (Priority: Pool > Subject > Default)
    for (const argDef of argList) {
      const isAffiliate = argDef.affiliate && (argDef.affiliate === '$out' || argDef.affiliate === '$in');
      if (!isAffiliate || params[argDef.name] !== undefined) continue;

      const type = argDef.type?.toLowerCase() || '';
      const fullType = type.startsWith('jot:') ? type : 'jot:' + type;
      const consumer = this.consumers[fullType.split('<')[0]];
      
      if (consumer) {
        // Offer BOTH pool and subject to the consumer
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper }, subject);
        if (res !== undefined) {
          if (argDef.const !== undefined && res !== argDef.const) throw new Error("Const mismatch");
          params[argDef.name] = res;
          
          // CRITICAL: If the consumer used the subject, we must clear it.
          // Since Pass 2 is for affiliates, if the pool didn't change but we got a result,
          // it must have come from the subject.
          const poolConsumedBefore = pool.filter(p => p.consumed).length;
          // (Consumer already updated pool if it used it)
          if (pool.filter(p => p.consumed).length === poolConsumedBefore) {
             subject = null;
          }
          continue;
        }
      }

      // 3. Try Default
      if (argDef.default !== undefined) {
        params[argDef.name] = argDef.default;
        continue;
      }
    }

    // FINAL CHECK: Ensure complete schema satisfaction
    for (const argDef of argList) {
        if (params[argDef.name] === undefined) {
            throw new Error(`Compiler Error: Missing required argument '${argDef.name}' for '${opName}'`);
        }
    }

    // Positive Violation: Check that all provided arguments were consumed.
    const unconsumed = pool.find(p => !p.consumed);
    if (unconsumed) throw new Error(`Compiler Error: Unconsumed argument in '${opName}'`);

    return params;
  }
}
