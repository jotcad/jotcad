import { normalizeSelector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Next-Gen Compiler
 * 
 * Transforms a generic Jot AST into deterministic VFS selectors
 * using a dynamic schema catalog.
 */
export class JotCompiler {
    constructor(vfs = null) {
        this.operators = new Map();
        this.vfs = vfs;
    }

    /**
     * Register an operator with its schema and path template.
     */
    registerOperator(name, config) {
        this.operators.set(name, config);
    }

    /**
     * Evaluate an AST node or value.
     */
    async evaluate(node) {
        if (node === null || node === undefined) return null;

        // 1. Handle Sequences (Arrays)
        if (Array.isArray(node)) {
            const results = [];
            for (const n of node) results.push(await this.evaluate(n));
            return this.flatten(results);
        }

        // 2. Handle Primitive Values
        if (typeof node !== 'object') {
            return node;
        }

        if (node.type === 'SYMBOL') {
            return node;
        }

        // 3. Handle CALL Nodes (e.g., box(10))
        if (node.type === 'CALL') {
            return await this._evaluateCall(node);
        }

        // 4. Handle METHOD Nodes (e.g., subject.offset(2))
        if (node.type === 'METHOD') {
            return await this._evaluateMethod(node);
        }

        // 5. Handle already-resolved Selectors or Objects
        if (node.path && node.parameters) {
            const resolvedParams = {};
            for (const [k, v] of Object.entries(node.parameters)) {
                resolvedParams[k] = await this.evaluate(v);
            }
            return this.applySequencePrinciple(node.path, resolvedParams);
        }

        const result = {};
        for (const [k, v] of Object.entries(node)) {
            result[k] = await this.evaluate(v);
        }
        return result;
    }

    async _evaluateCall(node) {
        const op = this.operators.get(node.name);
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;
        const returns = op ? op.returns : null;

        const args = [];
        for (const arg of node.args) args.push(await this.evaluate(arg));
        const parameters = this.mapArguments(args, schema, node.name);

        const result = this.applySequencePrinciple(path, parameters);

        // Generator Unrolling: If it's a generator op, resolve it now
        if (this.vfs && !Array.isArray(result) && returns?.type === 'array') {
            const data = await this.vfs.readData(result.path, result.parameters);
            if (Array.isArray(data)) return data;
        }

        return result;
    }

    async _evaluateMethod(node) {
        const subject = await this.evaluate(node.subject);
        
        // Universal Sequence Principle: If subject is a sequence, map over it
        if (Array.isArray(subject)) {
            const results = [];
            for (const s of subject) {
                results.push(await this.evaluate({ ...node, subject: s }));
            }
            return this.flatten(results);
        }

        const op = this.operators.get(node.name);
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;

        const args = [];
        for (const arg of node.args) args.push(await this.evaluate(arg));
        const parameters = this.mapArguments(args, schema, node.name);

        // Standard method chaining: wrap subject as '$in'
        parameters.$in = subject;

        // Handle Boolean Operations (and -> op/group)
        if (node.name === 'and') {
            parameters.$in = [subject, ...args];
        }

        return this.applySequencePrinciple(path, parameters);
    }

    /**
     * Map positional arguments to property names based on JSON Schema.
     */
    mapArguments(args, schema, opName) {
        let parameters = {};
        
        // 1. If single argument is an object, use it directly (named parameters)
        if (args.length === 1 && typeof args[0] === 'object' && !Array.isArray(args[0]) && args[0].type !== 'SYMBOL') {
            parameters = { ...args[0] };
        } else if (schema && (schema.arguments || schema.properties)) {
            // 2. Map positional arguments using schema definitions
            const argDefs = schema.arguments || schema.properties;
            const propNames = Object.keys(argDefs);
            args.forEach((arg, i) => {
                if (i < propNames.length) {
                    parameters[propNames[i]] = arg;
                }
            });
            
            // Apply defaults
            for (const [name, config] of Object.entries(argDefs)) {
                if (parameters[name] === undefined && config.default !== undefined) {
                    parameters[name] = config.default;
                }
            }
        } else {
            // Fallback if no schema: use generic 'args' array or first arg
            if (args.length === 1) parameters.arg = args[0];
            else if (args.length > 1) parameters.args = args;
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

        // 4. Variant routing (Special case for hexagon variants)
        if (opName === 'hexagon' && parameters.variant) {
            // This is slightly tricky as we return parameters here, not the path.
            // We'll let evaluateCall handle the path modification if variant is present.
        }

        return parameters;
    }

    /**
     * Implement the Universal Sequence Principle (Implicit Mapping + Cartesian Product).
     */
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
