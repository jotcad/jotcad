/**
 * Helper to convert a formal JOT Schema into a legacy flat Type Map.
 * Used to bridge existing tests to the new contract-first model.
 */
export function schemaToTypeMap(schema) {
    const typeMap = {};
    if (schema.arguments) {
        schema.arguments.forEach(arg => {
            const type = typeof arg === 'string' ? arg : arg.type;
            const name = typeof arg === 'string' ? arg : arg.name;
            typeMap[name] = type.startsWith('jot:') ? type : 'jot:' + type;
        });
    }
    if (schema.outputs) {
        Object.entries(schema.outputs).forEach(([name, def]) => {
            const type = typeof def === 'string' ? def : (def.type || 'jot:shape');
            typeMap[name] = type.startsWith('jot:') ? type : 'jot:' + type;
        });
    }
    return typeMap;
}
