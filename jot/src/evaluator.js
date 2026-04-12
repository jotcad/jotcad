import { normalizeSelector } from '../../fs/src/vfs_core.js';

/**
 * JotCAD Selector Evaluator
 * 
 * Resolves Late-Bound Symbols in a VFS selector using provided parameters.
 */
export class JotEvaluator {
    /**
     * Resolve all SYMBOL literals in a selector or value.
     */
    resolve(value, parameters = {}) {
        if (value && typeof value === 'object') {
            // Handle op/spec at the root or nested
            if (value.path === 'op/spec') {
                const spec = value.parameters.spec;
                const source = value.parameters.source;
                const canonicalParams = this.canonicalize(spec, parameters);
                // Resolve the source with the new canonical parameters
                return this.resolve(source, canonicalParams);
            }

            if (value.type === 'SYMBOL') {
                return parameters[value.name] !== undefined ? parameters[value.name] : value;
            }
            if (Array.isArray(value)) {
                return value.map(v => this.resolve(v, parameters));
            }
            if (value.path && value.parameters) {
                const resolvedParams = this.resolve(value.parameters, parameters);
                return normalizeSelector(value.path, resolvedParams);
            }
            const result = {};
            for (const [k, v] of Object.entries(value)) {
                result[k] = this.resolve(v, parameters);
            }
            return result;
        }
        return value;
    }

    /**
     * Canonicalize parameters based on a spec.
     */
    canonicalize(spec, params = {}) {
        const result = { ...params };
        for (const [key, config] of Object.entries(spec)) {
            // 1. Handle aliases
            if (config.alias) {
                const aliases = Array.isArray(config.alias) ? config.alias : [config.alias];
                for (const alias of aliases) {
                    if (params[alias] !== undefined && result[key] === undefined) {
                        result[key] = params[alias];
                    }
                }
            }
            // 2. Handle defaults
            if (result[key] === undefined && config.default !== undefined) {
                result[key] = config.default;
            }
        }
        return result;
    }
}
