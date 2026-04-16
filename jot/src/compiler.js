import { normalizeSelector } from '../../fs/src/vfs_core.js';

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
      result = this.applySequencePrinciple(node.path, resolvedParams, node.schema);
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

  async _evaluateCall(node, parameters, currentSubject = null) {
    let op = this.operators.get(node.name);
    const path = op ? op.path : `${this.options.defaultPrefix}${node.name}`;
    const schema = op ? op.schema : null;
    const returns = op ? op.returns : null;

    // Propagate context to arguments: Box(diameter()) -> Box(subject.diameter())
    const args = [];
    for (const arg of node.args)
      args.push(await this._evaluateRecursive(arg, parameters, currentSubject));

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

    const result = this.applySequencePrinciple(path, resolvedParams, schema);

    if (this.vfs && !Array.isArray(result) && returns?.type === 'array') {
      const data = await this.vfs.readData(result.path, result.parameters);
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

    let op = this.operators.get(node.name);
    if (!op) {
      for (const [name, config] of this.operators.entries()) {
        if (name.toLowerCase() === node.name.toLowerCase()) {
          op = config;
          break;
        }
      }
    }

    const path = op ? op.path : `op/${node.name}`;
    const schema = op ? op.schema : null;

    // A method ALWAYS provides its subject as context to its arguments
    const args = [];
    for (const arg of node.args)
      args.push(await this._evaluateRecursive(arg, parameters, subject));

    const inputKey = this._getInputKey(schema) || '$in';
    const resolvedParams = this.mapArguments(args, schema, node.name, true);

    // Map subject to the primary input
    resolvedParams[inputKey] = subject;

    if (node.name === 'and') {
      resolvedParams.$in = [subject, ...args];
    }

    const result = this.applySequencePrinciple(path, resolvedParams, schema);

    const isAlias = op?.metadata?.aliases?.['$out'] === '$in';
    if (this.options.optimizeAliases && isAlias) {
      const results = Array.isArray(result) ? result : [result];
      for (const r of results) {
        this.sideDemands.set(JSON.stringify(r), r);
      }
      return subject;
    }

    return result;
  }

  _matchType(val, type) {
    if (!type) return true;
    const t = type.toLowerCase();

    // 1. Numeric Consumers (num, nums, number)
    if (t === 'num' || t === 'nums' || t === 'number') {
      return typeof val === 'number';
    }

    // 2. String Consumers (str, string, tags)
    if (t === 'str' || t === 'string' || t === 'tags') {
      return typeof val === 'string' || (typeof val === 'object' && val !== null && val.type === 'SYMBOL');
    }

    // 3. Boolean Consumers (bool, boolean)
    if (t === 'bool' || t === 'boolean') {
      return typeof val === 'boolean';
    }

    // 4. VFS/Selector Consumers (jot, jots, shape, geometry, selector)
    if (t === 'jot' || t === 'jots' || t === 'shape' || t === 'geometry' || t === 'selector') {
      return typeof val === 'object' && val !== null && val.path;
    }

    // 5. Structural Consumers (vec3, intv, array)
    if (t === 'vec3') {
      return Array.isArray(val) && val.length === 3 && val.every(v => typeof v === 'number');
    }
    if (t === 'intv') {
      return typeof val === 'number' || (Array.isArray(val) && val.length === 2);
    }
    if (t === 'array') {
      return Array.isArray(val);
    }

    return true;
  }

  /**
   * Map positional arguments to property names based on JSON Schema.
   */
  mapArguments(args, schema, opName, hasSubject = false) {
    let parameters = {};
    const argDefs = schema ? schema.arguments || schema.properties : null;

    if (
      args.length === 1 &&
      typeof args[0] === 'object' &&
      args[0] !== null &&
      !Array.isArray(args[0]) &&
      args[0].type !== 'SYMBOL' &&
      !args[0].path
    ) {
      parameters = { ...args[0] };
    } else if (argDefs) {
      const propNames = Object.keys(argDefs);
      const propConfigs = Object.values(argDefs);
      
      let argIndex = 0;
      let subjectSkipped = !hasSubject;

      for (let i = 0; i < propNames.length; i++) {
        const name = propNames[i];
        const config = propConfigs[i];
        const t = config.type ? config.type.toLowerCase() : '';

        // If method call, skip the first 'jot' slot for subject
        if (!subjectSkipped && (t === 'jot' || t === 'shape' || t === 'selector' || t === 'jots')) {
          subjectSkipped = true;
          continue;
        }

        if (argIndex >= args.length) continue;

        const isGreedy = t === 'nums' || t === 'jots' || t === 'tags';
        if (isGreedy) {
          const matching = [];
          while (argIndex < args.length && this._matchType(args[argIndex], t)) {
            matching.push(args[argIndex]);
            argIndex++;
          }
          if (matching.length > 0) {
            if (t === 'tags') {
              const tagMap = {};
              matching.forEach(tag => {
                const key = (typeof tag === 'object' && tag.type === 'SYMBOL') ? tag.name : tag;
                tagMap[key] = true;
              });
              parameters[name] = tagMap;
            } else {
              parameters[name] = matching;
            }
          }
        } else {
          const val = args[argIndex];
          if (this._matchType(val, t)) {
            if (t === 'intv' && typeof val === 'number') {
              parameters[name] = [-val / 2, val / 2];
            } else {
              parameters[name] = val;
            }
            argIndex++;
          }
        }
      }
    } else {
      if (args.length === 1) parameters.arg = args[0];
      else if (args.length > 1) parameters.args = args;
    }

    // Apply Defaults
    if (argDefs) {
      for (const [name, config] of Object.entries(argDefs)) {
        if (parameters[name] === undefined && config.default !== undefined) {
          parameters[name] = config.default;
        }
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

  applySequencePrinciple(path, parameters, schema) {
    const argDefs = schema?.arguments || schema?.properties || {};

    const sequenceKeys = Object.keys(parameters).filter((k) => {
      const val = parameters[k];
      if (!Array.isArray(val)) return false;
      
      // Never expand VFS selectors (jots) as they are the items themselves
      if (val.length > 0 && val[0]?.path) return false;

      // Check schema: skip structural types that are represented as arrays
      const type = argDefs[k]?.type?.toLowerCase() || '';
      if (type === 'vec3' || type === 'intv' || type === 'tags') return false;

      // Greedy consumers (nums, jots) already harvested their arrays, don't re-expand
      if (type === 'nums' || type === 'jots') return false;

      return true;
    });

    if (sequenceKeys.length === 0) {
      return normalizeSelector(path, parameters);
    }

    let combos = [{}];
    for (const [k, v] of Object.entries(parameters)) {
      if (!sequenceKeys.includes(k)) {
        combos[0][k] = v;
      }
    }

    for (const key of sequenceKeys) {
      const nextCombos = [];
      for (const combo of combos) {
        const vals = parameters[key];
        for (const val of vals) {
          nextCombos.push({ ...combo, [key]: val });
        }
      }
      combos = nextCombos;
    }

    const selectors = combos.map((p) => normalizeSelector(path, p));
    return selectors.length === 1 ? selectors[0] : selectors;
  }

  flatten(arr) {
    return arr.reduce(
      (acc, val) =>
        Array.isArray(val) ? acc.concat(this.flatten(val)) : acc.concat(val),
      []
    );
  }
}
