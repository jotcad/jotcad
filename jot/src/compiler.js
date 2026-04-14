import { normalizeSelector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Next-Gen Compiler
 * 
 * Transforms a generic Jot AST into deterministic VFS selectors
 * using a dynamic schema catalog.
 */
export class JotCompiler {
    constructor() {
        this.operators = new Map();
    }

    /**
     * Register an operator with its schema and path template.
     * @param {string} name The Jot function/method name (e.g., 'box')
     * @param {object} config { path: 'shape/box', schema: { ... } }
     */
    registerOperator(name, config) {
        this.operators.set(name, config);
    }

    /**
     * Evaluate an AST node or value.
     */
    evaluate(node) {
        if (node === null || node === undefined) return null;

        // 1. Handle Sequences (Arrays)
        if (Array.isArray(node)) {
            const results = node.map(n => this.evaluate(n));
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
            return this._evaluateCall(node);
        }

        // 4. Handle METHOD Nodes (e.g., subject.offset(2))
        if (node.type === 'METHOD') {
            return this._evaluateMethod(node);
        }

        // 5. Handle already-resolved Selectors or Objects
        if (node.path && node.parameters) {
            const resolvedParams = {};
            for (const [k, v] of Object.entries(node.parameters)) {
                resolvedParams[k] = this.evaluate(v);
            }
            return this.applySequencePrinciple(node.path, resolvedParams);
        }

        const result = {};
        for (const [k, v] of Object.entries(node)) {
            result[k] = this.evaluate(v);
        }
        return result;
    }

    _evaluateCall(node) {
        const op = this.operators.get(node.name);
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;

        const parameters = this.mapArguments(node.args, schema);
        
        // Handle special routing like hexagon variant
        let finalPath = path;
        if (node.name === 'hexagon' && parameters.variant) {
            finalPath = `${path}/${parameters.variant}`;
            delete parameters.variant;
        } else if ((node.name === 'triangle' || node.name === 'tri') && schema === null) {
            // Fallback for triangle if no schema is loaded yet
            if (parameters.side !== undefined) finalPath = 'shape/triangle/equilateral';
            else if (parameters.angle !== undefined) finalPath = 'shape/triangle/sas';
            else if (parameters.a !== undefined) finalPath = 'shape/triangle/sss';
        }

        return this.applySequencePrinciple(finalPath, parameters);
    }

    _evaluateMethod(node) {
        const subject = this.evaluate(node.subject);
        
        // Universal Sequence Principle: If subject is a sequence, map over it
        if (Array.isArray(subject)) {
            return this.flatten(subject.map(s => this.evaluate({
                ...node,
                subject: s
            })));
        }

        const op = this.operators.get(node.name);
        const path = op ? op.path : `op/${node.name}`;
        const schema = op ? op.schema : null;

        const args = node.args.map(arg => this.evaluate(arg));
        const parameters = this.mapArguments(args, schema);

        // Standard method chaining: wrap subject as 'source'
        parameters.source = subject;

        // Handle Boolean Operations (and -> op/group)
        if (node.name === 'and') {
            parameters.sources = [subject, ...args];
            delete parameters.source;
        }

        return this.applySequencePrinciple(path, parameters);
    }

    /**
     * Map positional arguments to property names based on JSON Schema.
     */
    mapArguments(args, schema) {
        const parameters = {};
        
        // 1. If single argument is an object, use it directly (named parameters)
        if (args.length === 1 && typeof args[0] === 'object' && !Array.isArray(args[0]) && args[0].type !== 'SYMBOL') {
            return { ...args[0] };
        }

        // 2. Map positional arguments using schema properties
        if (schema && schema.properties) {
            const propNames = Object.keys(schema.properties);
            args.forEach((arg, i) => {
                if (i < propNames.length) {
                    parameters[propNames[i]] = arg;
                }
            });
            
            // Apply defaults
            for (const [name, config] of Object.entries(schema.properties)) {
                if (parameters[name] === undefined && config.default !== undefined) {
                    parameters[name] = config.default;
                }
            }
        } else {
            // Fallback if no schema: use generic 'args' array or first arg
            if (args.length === 1) parameters.arg = args[0];
            else if (args.length > 1) parameters.args = args;
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
