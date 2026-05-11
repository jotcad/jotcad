import { normalizeSelector, Selector, getSelectorKey } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Next-Gen Compiler
 */
export class JotCompiler {
  constructor(vfs = null, options = {}) {
    this.operators = new Map();
    this.vfs = vfs;
    this.options = { optimizeAliases: true, defaultPrefix: 'jot/', ...options };
    this.symbolTypes = {};

    // Terminal Tracking
    this.candidates = new Map(); // Group-CID -> Set of ports
    this.consumed = new Set();   // "Port-CID"
    this.selectorInstances = new Map(); // Group-CID -> Base Selector


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
    this.symbolTypes = {};
    for (const [k, v] of Object.entries(symbolTypes)) {
        this.symbolTypes[k] = v.startsWith('jot:') ? v : 'jot:' + v;
    }
    this.localSymbols = { ...parameters };
    this.candidates.clear();
    this.consumed.clear();
    this.selectorInstances.clear();

    try {
      if (Array.isArray(ast)) {
        for (const node of ast) {
          await this._evaluateRecursive(node, this.localSymbols, null);
        }
      } else {
        await this._evaluateRecursive(ast, this.localSymbols, null);
      }

      // Collect all terminals
      const terminals = [];
      for (const [groupKey, ports] of this.candidates.entries()) {
        const base = this.selectorInstances.get(groupKey);
        for (const port of ports) {
          const term = Selector.fromObject(base).withOutput(port);
          const portKey = await getSelectorKey(term);
          if (!this.consumed.has(portKey)) {
            terminals.push(term);
            console.log(`[JotCompiler] Terminal discovered: ${term.path}:${port} (CID: ${portKey.slice(0, 8)}...)`);
          }
        }
      }

      return terminals;
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
      case 'ASSIGNMENT': {
        const val = await this._evaluateRecursive(node.value, parameters, subject);
        this.localSymbols[node.name] = val;
        parameters[node.name] = val; // Propagate assigned values to scope
        return val;
      }
      case 'CALL': return this._dispatchCall(node, parameters, subject);
      case 'METHOD': return this._evaluateMethod(node, parameters, subject);
      case 'SYMBOL': return parameters[node.name] !== undefined ? parameters[node.name] : node;
      case 'ANNOTATED_ARG': return this._evaluateRecursive(node.value, parameters, subject);
      case 'RANGE': return this._evaluateRange(node, parameters, subject);
      default:
        const res = {};
        for (const [k, v] of Object.entries(node)) res[k] = await this._evaluateRecursive(v, parameters, subject);
        return res;
    }
  }

  async _evaluateRange(node, parameters, subject) {
    const start = node.start !== null ? await this._evaluateRecursive(node.start, parameters, subject) : 0;
    const end = node.end !== null ? await this._evaluateRecursive(node.end, parameters, subject) : null;
    const step = node.step !== null ? await this._evaluateRecursive(node.step, parameters, subject) : null;
    const count = node.count !== null ? await this._evaluateRecursive(node.count, parameters, subject) : null;

    if (typeof start !== 'number') {
        throw new Error(`Compiler Error: Range start must be a number. Got ${typeof start}`);
    }

    const results = [];
    
    if (count !== null) {
        if (typeof count !== 'number') throw new Error(`Compiler Error: Range count must be a number.`);
        
        let actualStep = step;
        if (actualStep === null) {
            if (end !== null) {
                // If end is provided, we divide the distance by count (Exclusive)
                actualStep = (end - start) / count;
            } else {
                // No end, no step -> default to 1
                actualStep = 1;
            }
        }

        // Generate EXACTLY count items to avoid epsilon drift
        for (let i = 0; i < count; i++) {
            results.push(start + i * actualStep);
        }
    } else {
        // Standard 'by' loop (requires either 'end' or a default 'end=1')
        const actualEnd = end !== null ? end : 1;
        const actualStep = step !== null ? step : 1;
        
        if (actualStep === 0) return [start];

        const epsilon = 1e-10;
        let current = start;
        const isIncreasing = actualStep >= 0;
        
        while (true) {
            if (isIncreasing) {
                if (node.inclusiveEnd) {
                    if (current > actualEnd + epsilon) break;
                } else {
                    if (current >= actualEnd - epsilon) break;
                }
            } else {
                if (node.inclusiveEnd) {
                    if (current < actualEnd - epsilon) break;
                } else {
                    if (current <= actualEnd + epsilon) break;
                }
            }

            results.push(current);
            current += actualStep;
            if (results.length > 10000) break;
        }
    }

    return results;
  }

  async _evaluateMethod(node, parameters, subject) {
    const s = await this._evaluateRecursive(node.subject, parameters, subject);
    
    // Universal Mapping Principle
    if (Array.isArray(s)) {
      const results = [];
      for (const item of s) {
        results.push(await this._dispatchCall(node, parameters, item));
      }
      return results;
    }

    return await this._dispatchCall(node, parameters, s);
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
        const sel = new Selector(op.path, params).withOutput(port);
        await this._trackSelector(sel);
        return sel;
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

      // --- New: Exact Type Match Pass (Resolves Symbols) ---
      const candidates = this._findCandidates(pool, argDef.name);
      for (const p of candidates) {
        const val = await evaluateHelper(p.node); // Resolves
        if (this._getTypeOfValue(val) === fullType) {
          params[argDef.name] = this._normalize(val, fullType);
          p.consumed = true;
          await this._consume(val);
          break;
        }
      }
      if (params[argDef.name] !== undefined) continue;

      const consumer = this.consumers[fullType.split('<')[0]];
      if (consumer) {
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper }, null);
        if (res !== undefined) {
          if (argDef.const !== undefined && res !== argDef.const) throw new Error("Const mismatch");
          params[argDef.name] = (res?.type === 'SYMBOL') ? { ...res, type: fullType } : res;
          await this._consume(res);
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

      // --- New: Exact Type Match Pass (Resolves Symbols) ---
      const candidates = this._findCandidates(pool, argDef.name);
      for (const p of candidates) {
        const val = await evaluateHelper(p.node); // Resolves
        if (this._getTypeOfValue(val) === fullType) {
          params[argDef.name] = this._normalize(val, fullType);
          p.consumed = true;
          await this._consume(val);
          break;
        }
      }
      if (params[argDef.name] !== undefined) continue;

      const consumer = this.consumers[fullType.split('<')[0]];
      
      if (consumer) {
        // Offer BOTH pool and subject to the consumer
        const res = await consumer(pool, argDef, { opName, evaluate: evaluateHelper }, subject);
        if (res !== undefined) {
          if (argDef.const !== undefined && res !== argDef.const) throw new Error("Const mismatch");
          params[argDef.name] = (res?.type === 'SYMBOL') ? { ...res, type: fullType } : res;
          await this._consume(res);
        }
      }

      if (params[argDef.name] === undefined && argDef.default !== undefined) {
        params[argDef.name] = argDef.default;
      }
    }

    // PASS 3: Validation
    for (const argDef of argList) {
        if (params[argDef.name] === undefined && argDef.default === undefined) {
             throw new Error(`Compiler Error: Missing required argument '${argDef.name}' for '${opName}'`);
        }
    }

    // PASS 4: Strict Overload Check (Ensure all provided arguments were consumed)
    for (const p of pool) {
      if (!p.consumed) {
         throw new Error(`Compiler Error: Argument ${p.nameHint ? `'${p.nameHint}' ` : ''}was not consumed by '${opName}'`);
      }
    }

    return params;
  }

  _findCandidates(pool, nameHint) {
    return pool.filter(p => !p.consumed && (!p.nameHint || p.nameHint === nameHint));
  }

  // --- Type Predicates ---

  _getTypeOfValue(val) {
    if (val instanceof Selector) return this._getSelectorOutputType(val);
    if (typeof val === 'number') return 'jot:number';
    if (typeof val === 'string') return 'jot:string';
    if (typeof val === 'boolean') return 'jot:boolean';
    if (val?.type === 'SYMBOL') return this.symbolTypes[val.name] || 'jot:any';
    if (Array.isArray(val)) {
        if (val.length === 3 && val.every(v => typeof v === 'number')) return 'jot:vec3';
        if (val.length >= 1 && val.length <= 2 && val.every(v => typeof v === 'number')) return 'jot:interval';
        return 'jot:list';
    }
    return 'jot:any';
  }

  _isSubtype(actual, expected) {
    if (actual === expected) return true;
    if (actual === 'jot:any' || expected === 'jot:any') return true;
    return false;
  }

  _normalize(val, targetType) {
    const type = targetType.startsWith('jot:') ? targetType : 'jot:' + targetType;
    if (val?.type === 'SYMBOL') return { ...val, type };

    if (type === 'jot:interval') {
      if (typeof val === 'number') return [-Math.abs(val) / 2, Math.abs(val) / 2];
      if (Array.isArray(val)) {
        if (val.length === 1) {
            const v = val[0];
            return v < 0 ? [v, 0] : [0, v];
        }
        if (val.length === 2) {
            return [Math.min(val[0], val[1]), Math.max(val[0], val[1])];
        }
      }
    }
    
    if (type === 'jot:flags') {
        if (typeof val === 'string') return { [val]: true };
        if (Array.isArray(val)) {
            const res = {};
            for (const v of val) if (typeof v === 'string') res[v] = true;
            return res;
        }
    }

    if (type === 'jot:shapes' && !Array.isArray(val)) return [val];
    if (type === 'jot:numbers' && !Array.isArray(val)) return [val];
    if (type === 'jot:strings' && !Array.isArray(val)) return [val];

    return val;
  }

  _checkSymbol(v, type, argDef, ctx) {
    const symType = this.symbolTypes[v.name];
    if (symType && !this._isSubtype(symType, type)) {
      // Compatibility: allow number in interval slot
      if (symType === 'jot:number' && type === 'jot:interval') return true;
      throw new Error(`Compiler Error: Symbol '${v.name}' (type ${symType}) used in ${type} slot '${argDef.name}' of '${ctx.opName}'`);
    }
    return true;
  }

  _getSelectorOutputType(sel) {
    if (!sel.output) {
      throw new Error(`Protocol Violation: Cannot determine type of port-less selector for "${sel.path}". Use .withOutput(port) to specify a value source.`);
    }

    const schema = this._getSchemaForPath(sel.path);
    if (!schema) {
      throw new Error(`Compiler Error: Unregistered operator path "${sel.path}" during type resolution.`);
    }

    const outDef = schema.outputs?.[sel.output];
    if (!outDef) {
      throw new Error(`Compiler Error: Port "${sel.output}" not found in schema for "${sel.path}". Available: ${Object.keys(schema.outputs || {}).join(', ')}`);
    }

    // Normalize: Treat 'shape' as 'jot:shape'
    const type = outDef.type || 'jot:any';
    return type.startsWith('jot:') ? type : 'jot:' + type;
  }

  _isJotNumber(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:number')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:number', a, c);
       return true;
    }
    return false;
  }

  _isJotString(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:string')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:string', a, c);
       return true;
    }
    return false;
  }

  _isJotBoolean(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:boolean')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:boolean', a, c);
       return true;
    }
    return false;
  }

  _isJotShape(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:shape')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:shape', a, c);
       return true;
    }
    if (v instanceof Selector) return true;
    if (typeof v === 'object' && v !== null && v.path) return true;
    return false;
  }

  _isJotShapeList(v, a, c) {
    if (Array.isArray(v)) return v.every(item => this._isJotShape(item, a, c));
    if (v?.type === 'SYMBOL' && this._isSubtype(this._getTypeOfValue(v), 'jot:shapes')) return true;
    return false;
  }

  _isJotVec3(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:vec3')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:vec3', a, c);
       return true;
    }
    return false;
  }

  _isJotInterval(v, a, c) {
    const t = this._getTypeOfValue(v);
    if (this._isSubtype(t, 'jot:interval') || t === 'jot:number') {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:interval', a, c);
       return true;
    }
    return false;
  }

  // --- Terminal Tracking ---

  async _trackSelector(sel) {
    if (!(sel instanceof Selector)) return;
    // Protocol Rule: Use base identity (path + parameters) for grouping all its ports
    const base = new Selector(sel.path, sel.parameters);
    const key = await getSelectorKey(base);
    
    if (!this.candidates.has(key)) {
      this.candidates.set(key, new Set());
      this.selectorInstances.set(key, base);
      
      const schema = this._getSchemaForPath(sel.path);
      if (schema?.outputs) {
        for (const port of Object.keys(schema.outputs)) {
          this.candidates.get(key).add(port);
        }
      } else {
        this.candidates.get(key).add('$out');
      }
      console.log(`[JotCompiler] Tracking: ${sel.path} (CID: ${key.slice(0, 8)}...)`);
    }
  }

  async _consumeSelector(sel) {
    if (!(sel instanceof Selector)) return;
    // Mark specific Port-CID as consumed
    const key = await getSelectorKey(sel);
    this.consumed.add(key);
    console.log(`[JotCompiler] Consumed: ${sel.path}:${sel.output || '$out'} (CID: ${key.slice(0, 8)}...)`);
  }

  async _consume(val) {
    if (val instanceof Selector) {
      await this._consumeSelector(val);
    } else if (Array.isArray(val)) {
      for (const item of val) {
        await this._consume(item);
      }
    } else if (typeof val === 'object' && val !== null && val.type !== 'SYMBOL') {
      for (const item of Object.values(val)) {
        await this._consume(item);
      }
    }
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

  _handleResult(val, type) {
    if (val?.type === 'SYMBOL') return { ...val, type };
    return val;
  }

  // --- Consumers ---

  async JotNumberConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotNumber(val, argDef, ctx)) { 
          p.consumed = true; 
          return val; 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotNumber(subject, argDef, ctx)) {
        return subject;
    }
    return undefined;
  }

  async JotNumbersConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);

        if (val instanceof Selector) {
            const type = this._getSelectorOutputType(val);
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
        if (this._isJotNumber(subject, argDef, ctx)) {
            results.push(subject);
        } else if (Array.isArray(subject) && subject.every(v => this._isJotNumber(v, argDef, ctx))) {
            results.push(...subject);
        }
    }
    return results.length > 0 ? results : undefined;
  }

  async JotStringConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotString(val, argDef, ctx)) { 
          p.consumed = true; 
          return val; 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotString(subject, argDef, ctx)) {
        return subject;
    }
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
        if (this._isJotBoolean(val, argDef, ctx)) { 
          p.consumed = true; 
          return val; 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotBoolean(subject, argDef, ctx)) {
        return subject;
    }
    return undefined;
  }

  async JotShapeConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotShape(val, argDef, ctx)) { 
          p.consumed = true; 
          return val; 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotShape(subject, argDef, ctx)) {
        return subject;
    }
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
            results.push(...(Array.isArray(val) ? val : [val]));
            p.consumed = true;
        } else break;
    }
    if (results.length === 0 && subject !== null) {
        if (this._isJotShape(subject, argDef, ctx)) {
            results.push(subject);
        } else if (this._isJotShapeList(subject, argDef, ctx)) {
            results.push(...(Array.isArray(subject) ? subject : [subject]));
        }
    }
    return results.length > 0 ? this._normalize(results, 'jot:shapes') : undefined;
  }

  async JotFlagsConsumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    const results = [];
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (typeof val === 'string' || val?.type === 'SYMBOL') {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    return results.length > 0 ? this._normalize(results, 'jot:flags') : undefined;
  }

  async JotVec3Consumer(pool, argDef, ctx, subject) {
    const candidates = this._findCandidates(pool, argDef.name);
    for (const p of candidates) {
        const val = await ctx.evaluate(p.node, false);
        if (this._isJotVec3(val, argDef, ctx)) { 
          p.consumed = true; 
          return val; 
        }
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
          return this._normalize(val, 'jot:interval');
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotInterval(subject, argDef, ctx)) {
        return this._normalize(subject, 'jot:interval');
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
    if (subject !== null) {
        return subject;
    }
    return undefined;
  }
}
