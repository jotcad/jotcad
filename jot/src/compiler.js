import { normalizeSelector, Selector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Next-Gen Compiler
 *
 * Transforms a generic Jot AST into deterministic VFS selectors
 * using a dynamic schema catalog.
 */
export class JotCompiler {
  constructor(vfs = null, options = {}) {
    this.operators = new Map();
    this.vfs = vfs;
    this.options = {
      optimizeAliases: true,
      defaultPrefix: 'jot/',
      ...options,
    };
    this.sideDemands = new Map();
  }

  /**
   * Register a local (functional) operator that returns data directly.
   */
  registerLocal(name, fn) {
    this.operators.set(name, { local: fn });
  }

  /**
   * Register an operator with its schema and path template.
   */
  registerOperator(name, config) {
    this.operators.set(name, config);
  }

  /**
   * Evaluate an AST node and return all resulting VFS demands.
   */
  async evaluate(node, parameters = {}) {
    this.sideDemands = new Map();
    const result = await this._evaluateRecursive(node, parameters, null, true);

    const sideValues = Array.from(this.sideDemands.values());
    if (sideValues.length === 0) {
      return result;
    }

    const all = [result, ...sideValues].filter(v => v !== null && v !== undefined);
    const flattened = this.flatten(all);
    return flattened.length === 1 ? flattened[0] : flattened;
  }

  async _evaluateRecursive(
    node,
    parameters = {},
    currentSubject = null,
    isTopLevel = false
  ) {
    if (node === null || node === undefined) return null;

    let result;
    // 1. Handle Sequences (Arrays)
    if (Array.isArray(node)) {
      const results = [];
      for (const n of node)
        results.push(
          await this._evaluateRecursive(n, parameters, currentSubject)
        );
      result = this.flatten(results);
    } else if (typeof node !== 'object') {
      // 2. Handle Primitive Values
      result = node;
    } else if (node.type === 'SYMBOL') {
      if (node.name === 'true') return true;
      if (node.name === 'false') return false;
      result =
        parameters[node.name] !== undefined ? parameters[node.name] : node;
    } else if (node.type === 'CALL') {
      // 3. Handle CALL Nodes (e.g., box(10))
      result = await this._evaluateCall(node, parameters, currentSubject);
    } else if (node.type === 'METHOD') {
      // 4. Handle METHOD Nodes (e.g., subject.offset(2))
      result = await this._evaluateMethod(node, parameters, currentSubject);
    } else if (node.type === 'ANNOTATED_ARG') {
      // Handle annotated arguments (Type:Name=Expression)
      const val = await this._evaluateRecursive(node.value, parameters, currentSubject);
      return { __annotated: true, typeHint: node.typeHint, nameHint: node.nameHint, value: val };
    } else if (node.path && node.parameters) {
      // 5. Handle already-resolved Selectors or Objects
      const resolvedParams = {};
      for (const [k, v] of Object.entries(node.parameters)) {
        resolvedParams[k] = await this._evaluateRecursive(
          v,
          parameters,
          currentSubject
        );
      }
      result = new Selector(node.path, resolvedParams, node.output);
    } else {
      result = {};
      for (const [k, v] of Object.entries(node)) {
        result[k] = await this._evaluateRecursive(
          v,
          parameters,
          currentSubject
        );
      }
    }

    return result;
  }

  _getInputKey(schema) {
    const argNames = schema?.arguments ? Object.keys(schema.arguments) : [];
    if (argNames.includes('$in')) return '$in';
    if (argNames.includes('source')) return 'source';
    if (argNames.includes('input')) return 'input';
    return null;
  }

  _resolveOperator(name) {
    // 1. Try exact match
    let op = this.operators.get(name);
    if (op) return op;

    // 2. Hierarchical resolution: find all candidates
    const targetPrefix = name + '/';
    const candidates = [];

    for (const [opKey, config] of this.operators.entries()) {
      if (opKey.startsWith(targetPrefix)) {
        candidates.push(config);
      }
    }
    
    return candidates;
  }

  async _evaluateCall(node, parameters, currentSubject = null) {
    let candidates = this._resolveOperator(node.name);
    if (!candidates || (Array.isArray(candidates) && candidates.length === 0)) {
      throw new Error(`Unregistered operator: ${node.name}.`);
    }

    // Propagate context to arguments: Box(diameter()) -> Box(subject.diameter())
    const args = [];
    for (const arg of node.args)
      args.push(await this._evaluateRecursive(arg, parameters, currentSubject));

    // Resolve variant based on schema matching if multiple candidates exist
    let op = null;
    if (Array.isArray(candidates)) {
      let bestMatch = null;
      let maxConstraints = -1;
      let ambiguous = false;

      for (const candidate of candidates) {
        const mapped = this.mapArguments(args, candidate.schema, node.name, false);
        let match = true;
        let constraintCount = 0;
        const argDefs = candidate.schema?.arguments || {};
        
        for (const [name, config] of Object.entries(argDefs)) {
          if (config.const !== undefined) {
            constraintCount++;
            if (mapped[name] !== config.const) {
              match = false;
              break;
            }
          }
        }
        
        if (match) {
          if (constraintCount > maxConstraints) {
            maxConstraints = constraintCount;
            bestMatch = candidate;
            ambiguous = false;
          } else if (constraintCount === maxConstraints) {
            ambiguous = true;
          }
        }
      }
      
      if (ambiguous || !bestMatch) {
        throw new Error(`Ambiguous or unregistered operator call: ${node.name}. Please provide parameters to disambiguate.`);
      }
      op = bestMatch;
    } else {
      op = candidates;
    }

    const path = op.path;
    const schema = op.schema;
    const returns = op.returns;

    const inputKey = this._getInputKey(schema);
    const hasActiveInput = currentSubject !== null && inputKey !== null;

    const resolvedParams = this.mapArguments(
      args,
      schema,
      node.name,
      hasActiveInput
    );

    // Apply Implicit Context if operator has a designated input slot
    if (hasActiveInput && resolvedParams[inputKey] === undefined) {
      resolvedParams[inputKey] = currentSubject;
    }

    const result = new Selector(path, resolvedParams);

    // Schema-Driven Port Affiliation: 
    // If the schema specifies an affiliate port for an argument, ensure the target selector uses it.
    const affiliate = (params, schema) => {
        if (!schema?.arguments) return;
        for (const [name, config] of Object.entries(schema.arguments)) {
            const port = config.affiliate;
            if (!port || params[name] === undefined) continue;

            const apply = (val) => {
                if (val instanceof Selector) {
                    if (!val.output) val.output = port;
                } else if (Array.isArray(val)) {
                    val.forEach(item => apply(item));
                }
            };
            apply(params[name]);
        }
    };
    affiliate(result.parameters, schema);

    if (this.vfs && !Array.isArray(result) && returns?.type === 'array') {
      const data = await this.vfs.readData(result);
      if (Array.isArray(data)) return data;
    }

    return result;
  }

  async _evaluateMethod(node, parameters, currentSubject = null) {
    const subject = await this._evaluateRecursive(
      node.subject,
      parameters,
      currentSubject
    );

    if (Array.isArray(subject)) {
      const results = [];
      for (const s of subject) {
        results.push(
          await this._evaluateRecursive(
            { ...node, subject: s },
            parameters,
            currentSubject
          )
        );
      }
      return this.flatten(results);
    }

    let candidates = this._resolveOperator(node.name);

    if (!candidates || (Array.isArray(candidates) && candidates.length === 0)) {
      // Dynamic fallback
      candidates = { path: this.options.defaultPrefix + node.name, schema: {} };
    }

    // A method provides its subject as context to its arguments ONLY for non-operation types
    const args = [];
    for (const arg of node.args)
      args.push(await this._evaluateRecursive(arg, parameters, null)); // Evaluated naked

    // Resolve variant based on schema matching
    let op = null;
    if (Array.isArray(candidates)) {
      let bestMatch = null;
      let maxConstraints = -1;
      let ambiguous = false;

      for (const candidate of candidates) {
        const mapped = this.mapArguments(args, candidate.schema, node.name, true);
        let match = true;
        let constraintCount = 0;
        const argDefs = candidate.schema?.arguments || {};

        for (const [name, config] of Object.entries(argDefs)) {
          if (config.const !== undefined) {
            constraintCount++;
            if (mapped[name] !== config.const) {
              match = false;
              break;
            }
          }
        }

        if (match) {
          if (constraintCount > maxConstraints) {
            maxConstraints = constraintCount;
            bestMatch = candidate;
            ambiguous = false;
          } else if (constraintCount === maxConstraints) {
            ambiguous = true;
          }
        }
      }

      if (ambiguous || !bestMatch) {
        throw new Error(`Ambiguous or unregistered method call: ${node.name}. Please provide parameters to disambiguate.`);
      }
      op = bestMatch;
    } else {
      op = candidates;
    }

    const path = op.path;
    const schema = op.schema;

    console.log(`[JotCompiler] Mapped arguments for ${node.name}:`, JSON.stringify(args));
    const resolvedParams = this.mapArguments(args, schema, node.name, true);
    console.log(`[JotCompiler] Resolved params (pre-binding) for ${node.name}:`, JSON.stringify(resolvedParams));

    // Contextual Binding: Bind subject to any shape/shapes that are missing their primary input
    const bindSubject = (val) => {
      if (typeof val === 'object' && val !== null && val.path && val.parameters && val.parameters.$in === undefined) {
          let op = null;
          for (const config of this.operators.values()) {
            if (config.path === val.path) { op = config; break; }
          }
          if (!op) op = this._resolveOperator(val.path.startsWith('jot/') ? val.path.slice(4) : val.path);

          const inputKey = this._getInputKey(op?.schema) || '$in';
          if (inputKey && val.parameters[inputKey] === undefined) {
            val.parameters[inputKey] = subject;
          }
      }
    };

    const inputKey = this._getInputKey(schema) || '$in';
    console.log(`[JotCompiler] Method Input Key for ${node.name}: ${inputKey}`);

    for (const [name, val] of Object.entries(resolvedParams)) {
        if (name === inputKey) continue; // don't bind to the main subject input itself again

        if (typeof val === 'object' && val !== null && val.path) {
           bindSubject(val);
        } else if (Array.isArray(val)) {
           val.forEach(v => {
             if (typeof v === 'object' && v !== null && v.path) bindSubject(v);
           });
        }
    }

    // Map subject to the primary input of the method itself
    resolvedParams[inputKey] = subject;
    console.log(`[JotCompiler] Final params for ${node.name}:`, JSON.stringify(resolvedParams));
    if (node.name === 'and') {
      resolvedParams.$in = [subject, ...args];
    }

    const result = new Selector(path, resolvedParams);

    const isTee = op?.schema?.metadata?.passthrough === true || op?.metadata?.passthrough === true || op?.metadata?.aliases?.['$out'] === '$in';
    
    if (this.options.optimizeAliases && isTee) {
      console.log(`[JotCompiler] Lifting passthrough operation: ${node.name}`);
      const results = Array.isArray(result) ? result : [result];
      for (const r of results) {
        this.sideDemands.set(JSON.stringify(r), r);
      }
      return subject;
    }

    return result;
  }

  /**
   * Namespaced Type Matching (jot:number, jot:shape, etc.)
   */
  _matchType(val, type, hint = null) {
    if (!type) return true;
    const t = type.toLowerCase();

    // 1. Strict Hint Check: If a hint exists, it MUST match the target type family
    if (hint) {
      const h = hint.toLowerCase();
      if (h === 'op' || h === 'operation') {
        if (t !== 'jot:operation') return false;
      } else if (h === 'shape' || h === 'shapes') {
        if (t !== 'jot:shape' && t !== 'jot:shapes') return false;
      } else if (h === 'num' || h === 'number' || h === 'n') {
        if (t !== 'jot:number' && t !== 'jot:numbers') return false;
      } else if (h === 'str' || h === 'string' || h === 's') {
        if (t !== 'jot:string' && t !== 'jot:strings') return false;
      }
    }

    // 2. Structural Matchers (Consume 1)
    if (t === 'jot:number') return typeof val === 'number';
    if (t === 'jot:string') return typeof val === 'string' || (typeof val === 'object' && val !== null && val.type === 'SYMBOL');
    if (t === 'jot:boolean') return typeof val === 'boolean';
    if (t === 'jot:shape') return typeof val === 'object' && val !== null && val.path;
    if (t === 'jot:operation') return typeof val === 'object' && val !== null && val.path;

    // 3. Plural Matchers (Consume All - matching element-wise)
    if (t === 'jot:numbers') return typeof val === 'number';
    if (t === 'jot:strings') return typeof val === 'string' || (typeof val === 'object' && val !== null && val.type === 'SYMBOL');
    if (t === 'jot:booleans') return typeof val === 'boolean';
    if (t === 'jot:shapes') return typeof val === 'object' && val !== null && val.path;

    // 4. Legacy / Internal types
    if (t === 'vec3') return Array.isArray(val) && val.length === 3 && val.every(v => typeof v === 'number');
    if (t === 'intv') return typeof val === 'number' || (Array.isArray(val) && val.length === 2);
    if (t === 'array') return Array.isArray(val);

    return true;
  }

  /**
   * Map positional and named arguments to property names based on JSON Schema.
   */
  mapArguments(args, schema, opName, hasSubject = false) {
    const parameters = {};
    const argDefs = schema ? schema.arguments || schema.properties || {} : {};
    const propNames = Object.keys(argDefs);

    // Object Spreading: If a single object is passed, map it directly.
    if (args.length === 1 && typeof args[0] === 'object' && args[0] !== null && !Array.isArray(args[0]) && !args[0].path && !args[0].__annotated) {
      Object.assign(parameters, args[0]);
    }
    
    // Preparation: Wrap un-annotated args to simplify logic
    const wrappedArgs = args.map(a => (a && a.__annotated ? a : { value: a }));
    const unassignedArgs = new Set(wrappedArgs.keys());

    // Phase 1: Named Assignment (Highest Priority)
    for (let i = 0; i < wrappedArgs.length; i++) {
       const arg = wrappedArgs[i];
       if (arg.nameHint && argDefs[arg.nameHint]) {
          parameters[arg.nameHint] = arg.value;
          unassignedArgs.delete(i);
       }
    }

    // Phase 2 & 3: Type-Aware Positional Sweep
    let argPointer = 0;
    let subjectSkipped = !hasSubject;

    for (let i = 0; i < propNames.length; i++) {
      const name = propNames[i];
      const config = argDefs[name];
      const t = config.type ? config.type.toLowerCase() : '';

      // Skip the first 'jot:shape' slot if method call (subject mapping handled by caller)
      if (!subjectSkipped && (t === 'jot:shape' || t === 'jot:shapes')) {
        subjectSkipped = true;
        continue;
      }

      // Skip if already assigned by name
      if (parameters[name] !== undefined) continue;

      // Find the next unassigned argument that matches this slot's type
      while (argPointer < wrappedArgs.length && !unassignedArgs.has(argPointer)) {
        argPointer++;
      }

      if (argPointer >= wrappedArgs.length) continue;

      const arg = wrappedArgs[argPointer];
      const isGreedy = t.endsWith('s') && t.startsWith('jot:');

      if (isGreedy) {
        const matching = [];
        // Greedy consume until we hit a mismatch
        while (argPointer < wrappedArgs.length) {
          if (!unassignedArgs.has(argPointer)) { argPointer++; continue; }
          
          const a = wrappedArgs[argPointer];
          if (this._matchType(a.value, t, a.typeHint)) {
            matching.push(a.value);
            unassignedArgs.delete(argPointer);
            argPointer++;
          } else {
            break; // Stop plural on mismatch
          }
        }
        if (matching.length > 0) parameters[name] = matching;
      } else {
        // Scalar slot: Match 1
        if (this._matchType(arg.value, t, arg.typeHint)) {
           parameters[name] = arg.value;
           unassignedArgs.delete(argPointer);
           argPointer++;
        }
      }
    }

    // Apply Defaults
    for (const [name, config] of Object.entries(argDefs)) {
      if (parameters[name] === undefined && config.default !== undefined) {
        parameters[name] = config.default;
      }
    }

    // Normalizations
    if (parameters.radius !== undefined) {
      if (parameters.diameter === undefined) parameters.diameter = parameters.radius * 2;
      delete parameters.radius;
    }
    if (parameters.apothem !== undefined && (opName === 'hexagon' || opName?.includes('hexagon'))) {
      if (parameters.diameter === undefined) parameters.diameter = (2 * parameters.apothem) / 0.86602540378;
      delete parameters.apothem;
    }

    return parameters;
  }

  flatten(arr) {
    return arr.reduce(
      (acc, val) =>
        Array.isArray(val) ? acc.concat(this.flatten(val)) : acc.concat(val),
      []
    );
  }
}
