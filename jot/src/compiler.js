import { normalizeSelector, Selector, getSelectorKey } from '../../fs/src/vfs_core.js';
import { log, warn, error } from '../../fs/src/log.js';
import { JotParser } from './parser.js';

/**
 * JotCAD Next-Gen Compiler
 */
export class JotCompiler {
  constructor(vfs = null, options = {}) {
    this.operators = new Map();
    this.vfs = vfs;
    this.options = { optimizeAliases: true, defaultPrefix: 'jot/', ...options };

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
    const hasArgs = config.schema && Array.isArray(config.schema.arguments);
    const hasInputs = config.schema && typeof config.schema.inputs === 'object' && config.schema.inputs !== null;

    if (!hasArgs && !hasInputs) {
       const path = config.path || name;
       const msg = `Compiler Error: Operator '${name}' (path: ${path}) must provide a formal schema with 'arguments' (array) or 'inputs' (object). Received: ${JSON.stringify(config.schema)}`;
       error(`[JotCompiler] FATAL REGISTRATION ERROR: ${msg}`);
       throw new Error(msg);
    }

    if (!config.path) config.path = name;
    const configVersion = this._parseVersion(config.path);

    const register = (n) => {
      // 1. Version-Qualified Names (user/Test:v74) always match exactly
      if (n.includes(':v')) {
          log(`[JotCompiler] Registering (Exact): ${n} -> ${config.path}`);
          this.operators.set(n, [config]);
          return;
      }

      // 2. Short Names (Test, user/Test) resolve to LATEST version
      if (config.path.startsWith('user/')) {
          const existing = this.operators.get(n);
          if (existing && existing.length > 0) {
              const currentVersion = this._parseVersion(existing[0].path);
              if (configVersion <= currentVersion) {
                  // Keep the existing newer version for this short-name mapping
                  return;
              }
          }
          log(`[JotCompiler] Registering (Latest): ${n} -> ${config.path}`);
          this.operators.set(n, [config]);
          return;
      }

      // 3. Standard Operators (built-ins) support multi-variant dispatch
      log(`[JotCompiler] Registering: ${n} -> ${config.path}`);
      const list = this.operators.get(n) || [];
      if (!list.includes(config)) {
        list.push(config);
        this.operators.set(n, list);
      }
    };

    register(name);
    register(config.path);

    // Support Base-Name mapping for ALL variants (e.g. jot/Hexagon/radius -> Hexagon)
    // Also handle versioned paths (e.g. user/Foot5:v2 -> Foot5)
    const fullPath = config.path;
    const pathNoVersion = fullPath.split(':')[0];
    const parts = pathNoVersion.split('/');
    
    if (parts.length >= 2) {
        const namespace = parts[0];
        const baseName = parts[1];
        if ((namespace === 'jot' || namespace === 'user') && baseName) {
            register(baseName);
            // Also register namespace/baseName (e.g. user/Test) as a short-name
            register(`${namespace}/${baseName}`);
        }
    }
    
    // Also handle direct variants like 'Hexagon/equilateral' (without prefix)
    const nameNoVersion = name.split(':')[0];
    if (nameNoVersion.includes('/') && !nameNoVersion.startsWith('jot/') && !nameNoVersion.startsWith('user/')) {
        const base = nameNoVersion.split('/')[0];
        if (base) register(base);
    }
  }

  _parseVersion(path) {
    const match = path.match(/:v(\d+)$/);
    return match ? parseInt(match[1], 10) : 0;
  }

  async evaluate(ast, parameters = {}, schema = null, contextName = null) {
    log(`[JotCompiler] Evaluating AST${contextName ? ` (${contextName})` : ''}:`, JSON.stringify(ast, null, 2));
    log(`[JotCompiler] Parameters${contextName ? ` (${contextName})` : ''}:`, JSON.stringify(parameters, null, 2));

    if (!schema || typeof schema !== 'object' || (!schema.outputs && !schema.arguments)) {
        throw new Error(`JotCompiler Error: A formal schema is now mandatory. Received: ${JSON.stringify(schema)}`);
    }

    const ctx = {
        symbolTypes: {},
        requiredOutputs: new Set(Object.keys(schema.outputs || {})),
        localSymbols: { ...parameters },
        opName: contextName || 'evaluate'
    };

    // Populate internal symbolTypes from formal schema for greedy matching
    if (schema.arguments) {
        schema.arguments.forEach(arg => {
            const type = typeof arg === 'string' ? arg : arg.type;
            const name = typeof arg === 'string' ? arg : arg.name;
            ctx.symbolTypes[name] = type.startsWith('jot:') ? type : 'jot:' + type;
        });
    }
    if (schema.outputs) {
        Object.entries(schema.outputs).forEach(([name, def]) => {
            const type = typeof def === 'string' ? def : (def.type || 'jot:shape');
            ctx.symbolTypes[name] = type.startsWith('jot:') ? type : 'jot:' + type;
        });
    }

    // 1. RESOLVE DEFAULTS: If a parameter is missing, use the schema default.
    // Shape defaults that are strings (Jot code) are evaluated as implicit assignments.
    if (schema?.arguments) {
        for (const arg of schema.arguments) {
            if (ctx.localSymbols[arg.name] === undefined && arg.default !== undefined) {
                const val = arg.default;
                const isShape = arg.type === 'shape' || arg.type === 'jot:shape' || arg.type === 'jot:shapes';
                if (typeof val === 'string' && isShape && val.trim()) {
                    // Implicit assignment to the argument name
                    const parser = new JotParser();
                    const subAst = parser.parse(val);
                    const node = { 
                        type: 'ASSIGNMENT', 
                        name: arg.name, 
                        value: Array.isArray(subAst) ? subAst[0] : subAst 
                    };
                    await this._evaluateRecursive(node, ctx.localSymbols, null, ctx);
                } else {
                    ctx.localSymbols[arg.name] = val;
                }
            }
        }
    }

    const topLevelNodes = Array.isArray(ast) ? [...ast] : [ast];

    try {
      // 1. VALIDATION: Every top-level statement MUST be an assignment
      for (const node of topLevelNodes) {
          if (node?.type !== 'ASSIGNMENT') {
              const codeSnippet = this.stringifyAST(node);
              const ctxStr = contextName ? ` in '${contextName}'` : '';
              const msg = `Compiler Error${ctxStr}: Each top-level statement in a User Operator must be an assignment (e.g., 'A = Op()' or 'Op() -> $out'). Found ${node?.type || typeof node}: "${codeSnippet}"`;
              log(`[JotCompiler] ${msg}`, node);
              throw new Error(msg);
          }
      }

      // EXECUTION: Evaluate assignments sequentially
      for (const node of topLevelNodes) {
          await this._evaluateRecursive(node, ctx.localSymbols, null, ctx);
      }

      const results = [];
      // PASS 1: Schema Fulfillment & Extraction
      if (schema.outputs) {
          for (const portName of Object.keys(schema.outputs)) {
              const val = ctx.localSymbols[portName];
              if (val !== undefined) {
                  const portDef = schema.outputs[portName];
                  if (val instanceof Selector) {
                      // Protocol Integrity: Preserve the actual output port of the selector 
                      // (e.g. .file) while flagging which schema port it fulfills.
                      results.push({ port: portName, selector: val, schema: portDef });
                  } else if (Array.isArray(val)) {
                      for (const item of val) {
                          if (item instanceof Selector) {
                              results.push({ port: portName, selector: item, schema: portDef });
                          }
                      }
                  }
              }
          }
      }

      // PASS 1.5: Missing Output Check
      if (ctx.requiredOutputs.size > 0) {
          const missing = Array.from(ctx.requiredOutputs).join(', ');
          throw new Error(`Compiler Error: Operator script failed to assign values to output port(s): ${missing}`);
      }

      if (results.length > 1) {
          log(`[JotCompiler] Warning: Evaluation produced ${results.length} terminal results.`, results);
      }

      return results;
    } finally {
      // Local context ensures no shared state cleanup is needed
    }
  }

  async _evaluateRecursive(node, parameters, subject, ctx) {
    if (node === null || node === undefined) return node;
    if (typeof node !== 'object') return node;
    if (Array.isArray(node)) {
        const results = [];
        for (const item of node) results.push(await this._evaluateRecursive(item, parameters, subject, ctx));
        return results;
    }

    switch (node.type) {
      case 'ASSIGNMENT': {
        const prevReserved = ctx.reservedName;
        ctx.reservedName = node.name;
        try {
          const val = await this._evaluateRecursive(node.value, parameters, subject, ctx);
          
          // Protocol Rule: If the assignment targets a schema-defined output port, 
          // propagate it to the top-level symbols (global scope) regardless of lexical depth.
          if (ctx.requiredOutputs && ctx.requiredOutputs.has(node.name)) {
              ctx.localSymbols[node.name] = val;
              ctx.requiredOutputs.delete(node.name);
          } else {
              parameters[node.name] = val; // Standard local propagation
          }
          return val;
        } finally {
          ctx.reservedName = prevReserved;
        }
      }
      case 'BLOCK': {
        const s = await this._evaluateRecursive(node.subject, parameters, subject, ctx);
        // Protocol Integrity: Create a fresh local scope for the block that inherits from current parameters.
        const blockParams = Object.create(parameters);
        const topLevelNodes = Array.isArray(node.body) ? node.body : [node.body];
        for (const innerNode of topLevelNodes) {
            await this._evaluateRecursive(innerNode, blockParams, s, ctx);
        }
        return s; // Subject passthrough
      }
      case 'CALL': return this._dispatchCall(node, parameters, subject, ctx);
      case 'METHOD': return this._evaluateMethod(node, parameters, subject, ctx);
      case 'OUTPUT_ACCESS': return this._evaluateOutputAccess(node, parameters, subject, ctx);
      case 'SYMBOL': 
        if (parameters[node.name] !== undefined) return parameters[node.name];
        // BLOCK RECURSION: If the symbol matches a variable we are currently trying to define, it's an error.
        if (ctx.requiredOutputs.has(node.name) || ctx.reservedName === node.name) {
            throw new Error(`Compiler Error: Undefined variable '${node.name}' (recursive reference detected)`);
        }
        // Otherwise, allow it as a late-bound symbol
        return node;
      case 'ANNOTATED_ARG': return this._evaluateRecursive(node.value, parameters, subject, ctx);
      case 'RANGE': return this._evaluateRange(node, parameters, subject, ctx);
      default:
        const res = {};
        for (const [k, v] of Object.entries(node)) res[k] = await this._evaluateRecursive(v, parameters, subject, ctx);
        return res;
    }
  }

  async _evaluateRange(node, parameters, subject, ctx) {
    const start = node.start !== null ? await this._evaluateRecursive(node.start, parameters, subject, ctx) : 0;
    const end = node.end !== null ? await this._evaluateRecursive(node.end, parameters, subject, ctx) : null;
    const step = node.step !== null ? await this._evaluateRecursive(node.step, parameters, subject, ctx) : null;
    const count = node.count !== null ? await this._evaluateRecursive(node.count, parameters, subject, ctx) : null;

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

  async _evaluateMethod(node, parameters, subject, ctx) {
    const s = await this._evaluateRecursive(node.subject, parameters, subject, ctx);
    
    // Universal Mapping Principle
    if (Array.isArray(s)) {
      const results = [];
      for (const item of s) {
        results.push(await this._dispatchCall(node, parameters, item, ctx));
      }
      return results;
    }

    return await this._dispatchCall(node, parameters, s, ctx);
  }

  async _evaluateOutputAccess(node, parameters, subject, ctx) {
    const s = await this._evaluateRecursive(node.subject, parameters, subject, ctx);
    
    if (Array.isArray(s)) {
      return s.map(item => (item instanceof Selector ? item.withOutput(node.output) : item));
    }

    if (s instanceof Selector) {
      return s.withOutput(node.output);
    }

    return s;
  }

  async _dispatchCall(node, parameters, subject, ctx) {
    const candidates = this._resolveOperator(node.name);
    if (!candidates || candidates.length === 0) throw new Error(`Compiler Error: Unregistered operator '${node.name}'`);

    for (const op of candidates) {
      try {
        const pool = node.args.map(a => ({ 
            node: a, 
            nameHint: a.type === 'ANNOTATED_ARG' ? a.nameHint : null, 
            consumed: false 
        }));
        const params = await this._satisfySchema(op.schema, pool, parameters, subject, node.name, ctx);
        
        log(`[JotCompiler] Dispatching: ${node.name} -> ${op.path}`);
        if (op.path.startsWith('user/')) {
          log(`[JotCompiler] Calling User Op: ${op.path}`, params);
        }

        const port = op.schema.outputs ? Object.keys(op.schema.outputs)[0] : '$out';
        const sel = new Selector(op.path, params).withOutput(port);
        return sel;
      } catch (e) {
        if (candidates.length === 1) throw e;
        continue;
      }
    }
    throw new Error(`Compiler Error: No variant of '${node.name}' satisfied the provided arguments.`);
  }

  _resolveOperator(name) {
    log(`[JotCompiler] Resolving: ${name}`);
    if (this.operators.has(name)) {
      log(`[JotCompiler]   Found direct: ${name}`);
      return this.operators.get(name);
    }
    if (this.operators.has('jot/' + name)) {
      log(`[JotCompiler]   Found jot prefix: ${name}`);
      return this.operators.get('jot/' + name);
    }
    if (this.operators.has('user/' + name)) {
      log(`[JotCompiler]   Found user prefix: ${name}`);
      return this.operators.get('user/' + name);
    }
    log(`[JotCompiler]   NOT FOUND: ${name}`);
    return null;
  }

  async _satisfySchema(schema, pool, parameters, subject, opName, ctx) {
    const params = {}, argList = schema?.arguments || [], inputList = schema?.inputs || {};
    let subjectConsumed = false;

    const evaluateHelper = async (node, subCtx = {}) => {
        return await this._evaluateRecursive(node, parameters, subject, { ...ctx, ...subCtx });
    };

    // PASS 0: Formal Inputs
    // MANDATE: Inputs ONLY consume the subject. They NEVER touch the argument pool.
    for (const [name, inputDef] of Object.entries(inputList)) {
        const type = inputDef.type?.toLowerCase() || 'jot:any';
        const fullType = type.startsWith('jot:') ? type : 'jot:' + type;
        const consumer = this.consumers[fullType.split('<')[0]];

        if (name === '$in' && subject) {
            if (consumer) {
                const res = await consumer([], inputDef, { ...ctx, fullType, evaluate: evaluateHelper }, subject);
                if (res !== undefined) {
                    params[name] = res;
                    subjectConsumed = true;
                }
            } else if (this._isSubtype(this._getTypeOfValue(subject, ctx), fullType)) {
                params[name] = subject;
                subjectConsumed = true;
            }
        }
    }

    // PASS 1: Explicit Binding for regular arguments
    for (const p of pool) {
        if (p.nameHint && !p.consumed) {
            const argDef = argList.find(a => a.name === p.nameHint);
            if (argDef) {
                const type = argDef.type?.toLowerCase() || 'jot:any';
                const fullType = type.startsWith('jot:') ? type : 'jot:' + type;
                const consumer = this.consumers[fullType.split('<')[0]];

                if (consumer) {
                    const res = await consumer(pool, argDef, { ...ctx, fullType, evaluate: evaluateHelper }, null);
                    if (res !== undefined) {
                        params[argDef.name] = res;
                    }
                } else {
                    const val = await evaluateHelper(p.node);
                    params[argDef.name] = this._normalize(val, fullType);
                    p.consumed = true;
                }
            }
        }
    }

    // PASS 2: Positional / Greedy / Defaults for regular arguments
    for (const argDef of argList) {
      if (params[argDef.name] !== undefined) continue;

      const type = argDef.type?.toLowerCase() || 'jot:any';
      const fullType = type.startsWith('jot:') ? type : 'jot:' + type;

      // Greedy Consumer Pass
      const consumer = this.consumers[fullType.split('<')[0]];
      if (consumer) {
        const sub = subjectConsumed ? null : subject;
        const res = await consumer(pool, argDef, { ...ctx, fullType, evaluate: evaluateHelper }, sub);
        if (res !== undefined) {
          params[argDef.name] = res;
          if (sub !== null) {
              subjectConsumed = true;
          }
        }
      }

      // Default Value Pass
      if (params[argDef.name] === undefined && argDef.default !== undefined) {
        params[argDef.name] = argDef.default;
      }
    }

    // PASS 3: Validation
    // Protocol Exception: Required inputs ($in) can be missing if the operator is being evaluated 
    // as a template (no subject) AND we are inside a context that permits it (higher-order op).
    const isInsideHigherOrder = ctx.allowTemplates === true;
    for (const [name, inputDef] of Object.entries(inputList)) {
        if (params[name] === undefined && !inputDef.optional && !isInsideHigherOrder) {
            throw new Error(`Compiler Error: Missing required input '${name}' for '${opName}'`);
        }
    }
    for (const argDef of argList) {
        if (params[argDef.name] === undefined && argDef.default === undefined && !argDef.optional) {
             throw new Error(`Compiler Error: Missing required argument '${argDef.name}' for '${opName}'`);
        }
    }

    // PASS 4: Strict Overload Check
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

  _getTypeOfValue(val, ctx) {
    if (val instanceof Selector) return this._getSelectorOutputType(val);
    if (typeof val === 'number') return 'jot:number';
    if (typeof val === 'string') return 'jot:string';
    if (typeof val === 'boolean') return 'jot:boolean';
    if (val?.type === 'SYMBOL') return ctx.symbolTypes[val.name] || 'jot:any';
    if (Array.isArray(val)) {
        if (val.length === 3 && val.every(v => typeof v === 'number')) return 'jot:vec3';
        if (val.length >= 1 && val.length <= 2 && val.every(v => typeof v === 'number')) return 'jot:interval';
        return 'jot:list';
    }
    return 'jot:any';
  }

  _isSubtype(actual, expected) {
    const a = (actual || 'jot:any').startsWith('jot:') ? actual : 'jot:' + actual;
    const e = (expected || 'jot:any').startsWith('jot:') ? expected : 'jot:' + expected;
    
    if (a === e) return true;
    if (a === 'jot:any' || e === 'jot:any') return true;
    
    // Singular -> Plural Promotion
    if (a === 'jot:shape' && e === 'jot:shapes') return true;
    if (a === 'jot:number' && e === 'jot:numbers') return true;
    if (a === 'jot:string' && e === 'jot:strings') return true;

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
    const symType = ctx.symbolTypes[v.name];
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
    const t = this._getTypeOfValue(v, c);
    if (this._isSubtype(t, 'jot:number')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:number', a, c);
       return true;
    }
    return false;
  }

  _isJotString(v, a, c) {
    const t = this._getTypeOfValue(v, c);
    if (this._isSubtype(t, 'jot:string')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:string', a, c);
       return true;
    }
    return false;
  }

  _isJotBoolean(v, a, c) {
    const t = this._getTypeOfValue(v, c);
    if (this._isSubtype(t, 'jot:boolean')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:boolean', a, c);
       return true;
    }
    return false;
  }

  _isJotShape(v, a, c) {
    const t = this._getTypeOfValue(v, c);
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
    if (v?.type === 'SYMBOL' && this._isSubtype(this._getTypeOfValue(v, c), 'jot:shapes')) return true;
    return false;
  }

  _isJotVec3(v, a, c) {
    const t = this._getTypeOfValue(v, c);
    if (this._isSubtype(t, 'jot:vec3')) {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:vec3', a, c);
       return true;
    }
    return false;
  }

  _isJotInterval(v, a, c) {
    const t = this._getTypeOfValue(v, c);
    if (this._isSubtype(t, 'jot:interval') || t === 'jot:number') {
       if (v?.type === 'SYMBOL') return this._checkSymbol(v, 'jot:interval', a, c);
       return true;
    }
    return false;
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
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotNumber(val, argDef, ctx)) { 
          p.consumed = true; 
          return this._normalize(val, ctx.fullType); 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    return (subject !== null && this._isJotNumber(subject, argDef, ctx)) ? this._normalize(subject, ctx.fullType) : undefined;
  }

  async JotNumbersConsumer(pool, argDef, ctx, subject) {
    const results = [];
    if (subject !== null && this._isJotNumber(subject, argDef, ctx)) {
        results.push(subject);
    }

    const startIndex = pool.findIndex(p => !p.consumed);
    if (startIndex !== -1) {
        for (let i = startIndex; i < pool.length; i++) {
            const p = pool[i];
            if (p.consumed) continue;
            if (p.nameHint && p.nameHint !== argDef.name) break;

            const val = await ctx.evaluate(p.node);
            if (this._isJotNumber(val, argDef, ctx)) {
                results.push(val);
                p.consumed = true;
            } else if (Array.isArray(val) && val.every(v => this._isJotNumber(v, argDef, ctx))) {
                results.push(...val);
                p.consumed = true;
            } else {
                break;
            }
        }
    }

    return results.length > 0 ? this._normalize(results, ctx.fullType) : undefined;
  }

  async JotStringConsumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotString(val, argDef, ctx)) { 
          p.consumed = true; 
          return this._normalize(val, ctx.fullType); 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    return (subject !== null && this._isJotString(subject, argDef, ctx)) ? this._normalize(subject, ctx.fullType) : undefined;
  }

  async JotStringsConsumer(pool, argDef, ctx, subject) {
    const results = [];
    if (subject !== null && this._isJotString(subject, argDef, ctx)) {
        results.push(subject);
    }

    const startIndex = pool.findIndex(p => !p.consumed);
    if (startIndex !== -1) {
        for (let i = startIndex; i < pool.length; i++) {
            const p = pool[i];
            if (p.consumed) continue;
            if (p.nameHint && p.nameHint !== argDef.name) break;

            const val = await ctx.evaluate(p.node);
            if (this._isJotString(val, argDef, ctx)) {
                results.push(val);
                p.consumed = true;
            } else break;
        }
    }
    return results.length > 0 ? this._normalize(results, ctx.fullType) : undefined;
  }

  async JotBooleanConsumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotBoolean(val, argDef, ctx)) { 
          p.consumed = true; 
          return this._normalize(val, ctx.fullType); 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    return (subject !== null && this._isJotBoolean(subject, argDef, ctx)) ? this._normalize(subject, ctx.fullType) : undefined;
  }

  async JotShapeConsumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotShape(val, argDef, ctx)) { 
          p.consumed = true; 
          return this._normalize(val, ctx.fullType); 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    return (subject !== null && this._isJotShape(subject, argDef, ctx)) ? this._normalize(subject, ctx.fullType) : undefined;
  }

  async JotShapesConsumer(pool, argDef, ctx, subject) {
    const results = [];
    if (subject !== null && (this._isJotShape(subject, argDef, ctx) || this._isJotShapeList(subject, argDef, ctx))) {
        if (Array.isArray(subject)) results.push(...subject);
        else results.push(subject);
    }

    const startIndex = pool.findIndex(p => !p.consumed);
    if (startIndex !== -1) {
        for (let i = startIndex; i < pool.length; i++) {
            const p = pool[i];
            if (p.consumed) continue;
            if (p.nameHint && p.nameHint !== argDef.name) break;

            const val = await ctx.evaluate(p.node);
            if (this._isJotShape(val, argDef, ctx)) {
                results.push(val);
                p.consumed = true;
            } else if (this._isJotShapeList(val, argDef, ctx)) {
                results.push(...(Array.isArray(val) ? val : [val]));
                p.consumed = true;
            } else break;
        }
    }
    return results.length > 0 ? this._normalize(results, ctx.fullType) : undefined;
  }

  async JotFlagsConsumer(pool, argDef, ctx, subject) {
    const results = [];
    const startIndex = pool.findIndex(p => !p.consumed);
    if (startIndex === -1) return undefined;

    for (let i = startIndex; i < pool.length; i++) {
        const p = pool[i];
        if (p.consumed) continue;
        if (p.nameHint && p.nameHint !== argDef.name) break;

        const val = await ctx.evaluate(p.node);
        if (typeof val === 'string' || val?.type === 'SYMBOL') {
            results.push(val);
            p.consumed = true;
        } else break;
    }
    return results.length > 0 ? this._normalize(results, ctx.fullType) : undefined;
  }

  async JotVec3Consumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotVec3(val, argDef, ctx)) { 
          p.consumed = true; 
          return this._normalize(val, ctx.fullType); 
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotVec3(subject, argDef, ctx)) return this._normalize(subject, ctx.fullType);
    return undefined;
  }

  async JotIntervalConsumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        const val = await ctx.evaluate(p.node);
        if (this._isJotInterval(val, argDef, ctx)) {
          p.consumed = true;
          return this._normalize(val, ctx.fullType);
        }
        if (p.nameHint === argDef.name) return undefined;
    }
    if (subject !== null && this._isJotInterval(subject, argDef, ctx)) {
        return this._normalize(subject, ctx.fullType);
    }
    return undefined;
  }

  async JotAnyConsumer(pool, argDef, ctx, subject) {
    const p = pool.find(p => !p.consumed);
    if (p) {
        if (p.nameHint && p.nameHint !== argDef.name) return undefined;
        
        // Protocol: If this is an operator-expecting port (template), allow missing required inputs 
        // in nested evaluation (they will be satisfied later by higher-order injection).
        const subCtx = (argDef.type || '').startsWith('jot:op') ? { allowTemplates: true } : {};
        const val = await ctx.evaluate(p.node, subCtx);
        p.consumed = true;
        return this._normalize(val, ctx.fullType);
    }
    if (subject !== null) {
        return this._normalize(subject, ctx.fullType);
    }
    return undefined;
  }

  stringifyAST(node) {
    if (node === null) return 'null';
    if (typeof node !== 'object') return String(node);
    
    switch (node.type) {
      case 'ASSIGNMENT':
        return `${this.stringifyAST(node.value)} -> ${node.name}`;
      case 'CALL':
        return `${node.name}(${(node.args || []).map(a => this.stringifyAST(a)).join(', ')})`;
      case 'METHOD':
        return `${this.stringifyAST(node.subject)}.${node.name}(${(node.args || []).map(a => this.stringifyAST(a)).join(', ')})`;
      case 'SYMBOL':
        return node.name;
      case 'ANNOTATED_ARG':
        let prefix = '';
        if (node.typeHint) prefix += `${node.typeHint}:`;
        if (node.nameHint) prefix += `${node.nameHint}=`;
        return `${prefix}${this.stringifyAST(node.value)}`;
      case 'OUTPUT_ACCESS':
        return `${this.stringifyAST(node.subject)}.${node.output}`;
      default:
        if (Array.isArray(node)) return `[${node.map(n => this.stringifyAST(n)).join(', ')}]`;
        return JSON.stringify(node);
    }
  }
}
