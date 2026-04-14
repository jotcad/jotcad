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
            ...options
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
    async evaluate(node) {
        this.sideDemands = new Map();
        const result = await this._evaluateRecursive(node, true);
        
        const sideValues = Array.from(this.sideDemands.values());
        if (sideValues.length === 0) {
            return result;
        }

        return this.flatten([result, ...sideValues]);
    }

    async _evaluateRecursive(node, isTopLevel = false) {
        if (node === null || node === undefined) return null;

        let result;
        // 1. Handle Sequences (Arrays)
        if (Array.isArray(node)) {
            const results = [];
            for (const n of node) results.push(await this._evaluateRecursive(n));
            result = this.flatten(results);
        } else if (typeof node !== 'object') {
            // 2. Handle Primitive Values
            result = node;
        } else if (node.type === 'SYMBOL') {
            result = node;
        } else if (node.type === 'CALL') {
            // 3. Handle CALL Nodes (e.g., box(10))
            result = await this._evaluateCall(node);
        } else if (node.type === 'METHOD') {
            // 4. Handle METHOD Nodes (e.g., subject.offset(2))
            result = await this._evaluateMethod(node);
        } else if (node.path && node.parameters) {
            // 5. Handle already-resolved Selectors or Objects
            const resolvedParams = {};
            for (const [k, v] of Object.entries(node.parameters)) {
                resolvedParams[k] = await this._evaluateRecursive(v);
            }
            result = this.applySequencePrinciple(node.path, resolvedParams);
        } else {
            result = {};
            for (const [k, v] of Object.entries(node)) {
                result[k] = await this._evaluateRecursive(v);
            }
        }

        return result;
    }

    async _evaluateCall(node) {
        // Strict case-sensitive lookup for registered operator
        let op = this.operators.get(node.name);
        
        // 2. Resolve Path: If registered, use that path. If not, default to op/name
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;
        const returns = op ? op.returns : null;

        const args = [];
        for (const arg of node.args) args.push(await this._evaluateRecursive(arg));
        const parameters = this.mapArguments(args, schema, node.name);

        const result = this.applySequencePrinciple(path, parameters);

        // Generator Unrolling: Fetch data if marked as array return
        if (this.vfs && !Array.isArray(result) && returns?.type === 'array') {
            const data = await this.vfs.readData(result.path, result.parameters);
            if (Array.isArray(data)) return data;
        }

        return result;
    }

    async _evaluateMethod(node) {
        const subject = await this._evaluateRecursive(node.subject);
        
        // Universal Sequence Principle: Map over sequences
        if (Array.isArray(subject)) {
            const results = [];
            for (const s of subject) {
                results.push(await this._evaluateRecursive({ ...node, subject: s }));
            }
            return this.flatten(results);
        }

        // 1. Operator lookup (try exact, then case-insensitive)
        let op = this.operators.get(node.name);
        if (!op) {
            for (const [name, config] of this.operators.entries()) {
                if (name.toLowerCase() === node.name.toLowerCase()) {
                    op = config;
                    break;
                }
            }
        }

        // 2. Determine Path
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;

        const args = [];
        for (const arg of node.args) args.push(await this._evaluateRecursive(arg));
        const parameters = this.mapArguments(args, schema, node.name);

        parameters.$in = subject;

        // Handle Boolean Operations (and -> op/group)
        if (node.name === 'and') {
            parameters.$in = [subject, ...args];
        }

        const result = this.applySequencePrinciple(path, parameters);

        // Alias Resolution (Optimization)
        const isAlias = op?.metadata?.aliases?.['$out'] === '$in';
        if (this.options.optimizeAliases && isAlias) {
            // Record the side effect demand (de-duplicated)
            const results = Array.isArray(result) ? result : [result];
            for (const r of results) {
                this.sideDemands.set(JSON.stringify(r), r);
            }
            return subject;
        }

        return result;
    }

    /**
     * Map positional arguments to property names based on JSON Schema.
     */
    mapArguments(args, schema, opName) {
        let parameters = {};
        
        const argDefs = schema ? (schema.arguments || schema.properties) : null;

        // 1. If single argument is an object, use it directly (named parameters)
        if (args.length === 1 && typeof args[0] === 'object' && args[0] !== null && !Array.isArray(args[0]) && args[0].type !== 'SYMBOL') {
            parameters = { ...args[0] };
        } else if (argDefs) {
            // 2. Map positional arguments using schema definitions
            const propNames = Object.keys(argDefs);
            args.forEach((arg, i) => {
                if (i < propNames.length) {
                    parameters[propNames[i]] = arg;
                }
            });
        } else {
            // Fallback if no schema: use generic 'arg' or 'args'
            if (args.length === 1) parameters.arg = args[0];
            else if (args.length > 1) parameters.args = args;
        }

        // Apply defaults for missing parameters
        if (argDefs) {
            for (const [name, config] of Object.entries(argDefs)) {
                if (parameters[name] === undefined && config.default !== undefined) {
                    parameters[name] = config.default;
                }
            }
        }

        // 3. Primacy of Diameter (Standardization)
        // Convert radius -> diameter
        if (parameters.radius !== undefined) {
            if (parameters.diameter === undefined) parameters.diameter = parameters.radius * 2;
            delete parameters.radius;
        }

        // Convert apothem -> diameter (for hexagon specifically)
        if (parameters.apothem !== undefined && (opName === 'hexagon' || opName?.includes('hexagon'))) {
            // d = 2 * a / cos(30)
            if (parameters.diameter === undefined) parameters.diameter = (2 * parameters.apothem) / 0.86602540378;
            delete parameters.apothem;
        }

        return parameters;
    }

    /**
     * Implement the Universal Sequence Principle (Implicit Mapping + Cartesian Product).
     */
    applySequencePrinciple(path, parameters) {
        // Find all parameters that are sequences (arrays)
        // We only expand arrays that ARE NOT nested selectors (which would have a 'path')
        const sequenceKeys = Object.keys(parameters).filter(k => 
            Array.isArray(parameters[k]) && (parameters[k].length === 0 || !parameters[k][0]?.path)
        );

        if (sequenceKeys.length === 0) {
            return normalizeSelector(path, parameters);
        }

        // Cartesian Product Generation
        let combos = [{}];
        // Start with non-sequence params
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

        const selectors = combos.map(p => normalizeSelector(path, p));
        return selectors.length === 1 ? selectors[0] : selectors;
    }

    /**
     * Recursively flatten nested sequences.
     */
    flatten(arr) {
        return arr.reduce((acc, val) => 
            Array.isArray(val) ? acc.concat(this.flatten(val)) : acc.concat(val), 
        []);
    }
}
