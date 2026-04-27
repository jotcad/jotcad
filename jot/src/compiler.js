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
      'jot:number': (p, a, c) => this.JotNumberConsumer(p, a, c),
      'jot:numbers': (p, a, c) => this.JotNumbersConsumer(p, a, c),
      'jot:string': (p, a, c) => this.JotStringConsumer(p, a, c),
      'jot:strings': (p, a, c) => this.JotStringsConsumer(p, a, c),
      'jot:boolean': (p, a, c) => this.JotBooleanConsumer(p, a, c),
      'jot:shape': (p, a, c) => this.JotShapeConsumer(p, a, c),
      'jot:shapes': (p, a, c) => this.JotShapesConsumer(p, a, c),
      'jot:flags': (p, a, c) => this.JotFlagsConsumer(p, a, c),
      'jot:vec3': (p, a, c) => this.JotVec3Consumer(p, a, c),
      'jot:interval': (p, a, c) => this.JotIntervalConsumer(p, a, c),
      'jot:operation': (p, a, c) => this.JotAnyConsumer(p, a, c),
      'jot:op': (p, a, c) => this.JotAnyConsumer(p, a, c),
      'jot:selector': (p, a, c) => this.JotAnyConsumer(p, a, c),
      'jot:any': (p, a, c) => this.JotAnyConsumer(p, a, c),
      'any': (p, a, c) => this.JotAnyConsumer(p, a, c),
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

  _isJotNumber(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:number', a, c);
    return typeof v === 'number';
  }

  _isJotString(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:string', a, c);
    return typeof v === 'string';
  }

  _isJotBoolean(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:boolean', a, c);
    return typeof v === 'boolean';
  }

  _isJotShape(v, a, c) {
    if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:shape', a, c);
    return typeof v === 'object' && v !== null && v.path;
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
    const configs = this.operators.get(path);
    if (configs) return configs[0].schema;
    const prefixedConfigs = this.operators.get(this.options.defaultPrefix + path);
    return prefixedConfigs?.[0]?.schema;
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

  async JotNumberConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotNumber(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    return undefined;
  }

  async JotNumbersConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    if (candidates.length === 0) return undefined;
    const p0 = candidates[0];
    let val0 = await ctx.evaluate(p0.node, false);
    if (Array.isArray(val0) && val0.every(v => this._isJotNumber(v, argDef, ctx))) {
        p0.consumed = true; return val0;
    }
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotNumber(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    return results.length > 0 ? results : undefined;
  }

  async JotStringConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotString(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    return undefined;
  }

  async JotStringsConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotString(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    return results.length > 0 ? results : undefined;
  }

  async JotBooleanConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotBoolean(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    return undefined;
  }

  async JotShapeConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotShape(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    return undefined;
  }

  async JotShapesConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotShape(val, argDef, ctx)) {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    return results.length > 0 ? results : undefined;
  }

  async JotFlagsConsumer(pool, argDef, ctx) {
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

  async JotVec3Consumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotVec3(val, argDef, ctx)) { p.consumed = true; return val; }
        if (p.nameHint === argDef.name) return undefined;
    }
    return undefined;
  }

  async JotIntervalConsumer(pool, argDef, ctx) {
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
    return undefined;
  }

  async JotAnyConsumer(pool, argDef, ctx) {
    const candidates = this._findCandidates(pool, argDef.name);
    if (candidates.length === 0) return undefined;
    const p = candidates[0];
    
    const typeStr = argDef.type?.toLowerCase() || '';
    const isTemplate = ['jot:operation', 'jot:selector', 'op', 'template', 'jot:op'].some(t => typeStr.startsWith(t));
    
    const val = await ctx.evaluate(p.node, isTemplate);
    
    if (isTemplate && typeStr.includes('<') && val instanceof Selector) {
        const signature = this._parseSignature(typeStr);
        if (signature) {
            const schema = this._getSchemaForPath(val.path);
            if (schema) {
                // 1. Output Validation
                const outPort = val.output || '$out';
                const actualOutType = schema.outputs?.[outPort]?.type || 'jot:any';
                if (actualOutType !== signature.output && signature.output !== 'jot:any') {
                    throw new Error(`Compiler Error: Template mismatch for '${argDef.name}'. Expected return type '${signature.output}', got '${actualOutType}' from '${val.path}'`);
                }

                // 2. Input Validation (Hole Check)
                for (const [key, type] of Object.entries(signature.inputs)) {
                    const slot = (schema.arguments || []).find(a => a.name === key || (key === '$in' && (a.affiliate === '$out' || a.affiliate === '$in')));
                    if (!slot) throw new Error(`Compiler Error: Template '${val.path}' has no slot to receive input '${key}'`);
                    if (val.parameters[slot.name] !== undefined) throw new Error(`Compiler Error: Template input '${key}' is already bound in '${val.path}'`);
                }
            }
        }
    }

    p.consumed = true;
    return val;
  }

  // --- Core Methods ---

  registerOperator(name, config) {
    let list = this.operators.get(name);
    if (!list) {
        list = [];
        this.operators.set(name, list);
    }
    list.push(config);
  }

  _resolveOperator(name) {
    let ops = this.operators.get(name) || [];
    const prefixed = this.options.defaultPrefix + name;
    const prefixedOps = this.operators.get(prefixed) || [];
    let candidates = [...ops, ...prefixedOps];

    const targetPrefix = name + '/', prefixedTarget = (this.options.defaultPrefix || '') + name + '/';
    for (const [key, list] of this.operators.entries()) {
      if (key.startsWith(targetPrefix) || key.startsWith(prefixedTarget)) {
          candidates.push(...list);
      }
    }
    return candidates;
  }

  async evaluate(node, parameters = {}, symbolTypes = {}) {
    this.sideDemands = new Map();
    this.symbolTypes = symbolTypes;
    return await this._evaluateRecursive(node, parameters, null);
  }

  async _evaluateRecursive(node, parameters, subject) {
    if (node === undefined || node === null) return null;
    if (Array.isArray(node)) return Promise.all(node.map(n => this._evaluateRecursive(n, parameters, subject)));
    if (typeof node !== 'object') return node;
    if (node.type === 'SYMBOL') {
      if (node.name === 'true') return true;
      if (node.name === 'false') return false;
      return parameters[node.name] !== undefined ? parameters[node.name] : node;
    }
    if (node.type === 'CALL') return this._dispatchCall(node, parameters, subject, false);
    if (node.type === 'METHOD') return this._evaluateMethod(node, parameters, subject);
    if (node.type === 'ANNOTATED_ARG') {
      return await this._evaluateRecursive(node.value, parameters, subject);
    }
    if (node.path && node.parameters) {
      const params = {};
      for (const [k, v] of Object.entries(node.parameters)) params[k] = await this._evaluateRecursive(v, parameters, subject);
      const candidates = this._resolveOperator(node.path);
      const op = candidates[0];
      const port = node.output || (op?.schema ? Object.keys(op.schema.outputs)[0] : '$out');
      return new Selector(node.path, params).withOutput(port);
    }
    const result = {};
    for (const [k, v] of Object.entries(node)) result[k] = await this._evaluateRecursive(v, parameters, subject);
    return result;
  }

  async _evaluateMethod(node, parameters, currentSubject) {
    const subject = await this._evaluateRecursive(node.subject, parameters, currentSubject);
    if (Array.isArray(subject)) return Promise.all(subject.map(s => this._evaluateMethod({ ...node, subject: s }, parameters, currentSubject)));
    return this._dispatchCall(node, parameters, subject, false);
  }

  async _dispatchCall(node, parameters, subject, isTemplateMode = false) {
    const candidates = this._resolveOperator(node.name);
    if (!candidates || candidates.length === 0) throw new Error(`Compiler Error: Unregistered operator '${node.name}'`);

    for (const op of candidates) {
      try {
        const pool = node.args.map(a => ({ 
            node: a, 
            nameHint: a.type === 'ANNOTATED_ARG' ? a.nameHint : null, 
            consumed: false 
        }));
        const params = await this._satisfySchema(op.schema, pool, parameters, subject, node.name, isTemplateMode);
        const port = Object.keys(op.schema.outputs)[0];
        return new Selector(op.path, params).withOutput(port);
      } catch (e) {
        if (candidates.length === 1) throw e;
        continue;
      }
    }
    throw new Error(`Compiler Error: No matching variant for '${node.name}' satisfies the provided arguments.`);
  }

  async _satisfySchema(schema, pool, parameters, subject, opName, isTemplateMode = false) {
    const params = {}, argList = schema?.arguments || [];

    const evaluateHelper = async (node, asTemplate) => {
        const sub = null;
        if (asTemplate) {
            if (node.type === 'CALL') return this._dispatchCall(node, parameters, sub, true);
            if (node.type === 'METHOD') {
                const s = await this._evaluateRecursive(node.subject, parameters, sub);
                return this._dispatchCall(node, parameters, s, true);
            }
            return this._evaluateRecursive(node, parameters, sub);
        }
        return this._evaluateRecursive(node, parameters, sub);
    };

    // PASS 1: Fill Non-Affiliate slots from the Pool
    for (const argDef of argList) {
      const isAffiliate = argDef.affiliate && (argDef.affiliate === '$out' || argDef.affiliate === '$in');
      if (isAffiliate) continue;

      const consumer = this.consumers[argDef.type?.toLowerCase().split('<')[0]];
      if (consumer) {
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper });
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

      // 1. Try Pool
      const consumer = this.consumers[argDef.type?.toLowerCase().split('<')[0]];
      if (consumer) {
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper });
        if (res !== undefined) {
          if (argDef.const !== undefined && res !== argDef.const) throw new Error("Const mismatch");
          params[argDef.name] = res;
          continue;
        }
      }

      // 2. Try Subject fallback
      if (subject !== null && subject !== undefined) {
        params[argDef.name] = subject;
        subject = null; // Mark subject as consumed
        continue;
      }

      // 3. Try Default
      if (argDef.default !== undefined) {
        params[argDef.name] = argDef.default;
        continue;
      }
    }

    // FINAL CHECK: Ensure complete schema satisfaction if NOT in Template Mode.
    if (!isTemplateMode) {
        for (const argDef of argList) {
            if (params[argDef.name] === undefined) {
                throw new Error(`Compiler Error: Missing required argument '${argDef.name}' for '${opName}'`);
            }
        }
    }

    // Positive Violation: Check that all provided arguments were consumed.
    const unconsumed = pool.find(p => !p.consumed);
    if (unconsumed) throw new Error(`Compiler Error: Unconsumed argument in '${opName}'`);

    return params;
  }
}
