import { JotParser } from './parser.js';
import { JotEvaluator } from './evaluator.js';

/**
 * Registers a VFS provider that evaluates .jot expressions.
 */
export function registerJotProvider(vfs) {
    const parser = new JotParser();
    const evaluator = new JotEvaluator();

    vfs.registerProvider('jot/eval', async (v, selector, context) => {
        const { expression, params = {} } = selector.parameters;
        if (!expression) return null;

        try {
            // 1. Parse the expression
            const ast = parser.parse(expression);
            
            // 2. Resolve symbols using provided parameters
            const resolvedSelector = evaluator.resolve(ast, params);
            
            // 3. Chain the request to the mesh
            if (Array.isArray(resolvedSelector)) {
                // If it's a sequence, we might need a group/sequence op
                // For now, let's assume we want the first one or a group
                return await v.read('op/group', { sources: resolvedSelector }, context);
            }
            return await v.read(resolvedSelector.path, resolvedSelector.parameters, context);

        } catch (err) {
            console.error(`[JotProvider] Evaluation error:`, err);
            return null;
        }
    });

    /**
     * Fallback provider for any .jot file request.
     * Maps vfs:/path/to/file.jot?p=v -> jot/eval({ expression: <content of file.jot>, params: { p: v } })
     */
    vfs.registerProvider('.jot', async (v, selector, context) => {
        if (!selector.path.endsWith('.jot')) return null;

        try {
            // 1. Read the raw .jot file content from the mesh
            // We strip parameters to get the base file
            const expression = await v.readText(selector.path, {}, context);
            if (!expression) return null;

            // 2. Delegate to jot/eval with the current parameters
            return await v.read('jot/eval', { expression, params: selector.parameters }, context);

        } catch (err) {
            console.error(`[JotProvider] File error for ${selector.path}:`, err);
            return null;
        }
    });
}
