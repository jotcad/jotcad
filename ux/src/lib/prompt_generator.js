/**
 * Translates a catalog of VFS schemas into a structured prompt prefix
 * to instruct an LLM on how to write valid JotCAD script.
 * 
 * @param {Object} schemas - Map of VFS path to schema object
 * @returns {string} The markdown-formatted prompt prefix
 */
export function generatePromptPrefix(schemas) {
    let prompt = `# JOTCAD DSL CODING INSTRUCTIONS\n\n`;
    prompt += `You are an expert JotCAD code generator. Your task is to translate natural language descriptions into a valid JotCAD DSL script.\n\n`;
    
    prompt += `## DSL Grammar and Rules:\n`;
    prompt += `- **PascalCase Constructors**: Used for creating basic shapes (e.g., Box(width, height), Cylinder(diameter, height)).\n`;
    prompt += `- **camelCase Operations/Methods**: Chained onto existing shapes (e.g., shape.extrude(10), shape.rotateZ(0.25)).\n`;
    prompt += `- **Turns for Rotation**: All rotations are specified in **Turns** (tau), where 1.0 represents a full 360-degree rotation (e.g., 90 degrees is 0.25 turns).\n`;
    prompt += `- **Interval Parameters**: Dimensions like width, height, or diameter accept intervals. E.g., 10 creates a symmetric shape centered at origin [-5, 5], whereas [10] creates an asymmetric shape starting at 0 [0, 10].\n`;
    prompt += `- **Explicit Output Wiring**: You MUST wire the final result to the output port using the arrow operator (\`-> $out\`).\n`;
    prompt += `- **No control flow**: There are no loops or conditions. Sequential replication is handled via list inputs or higher-order operators like \`at\`.\n\n`;
    
    prompt += `## Available Operators Reference:\n\n`;
    
    for (const [path, schema] of Object.entries(schemas)) {
        // Only document public jot/ and user/ operators
        if (!path.startsWith('jot/') && !path.startsWith('user/')) continue;
        
        const dslName = schema.dsl_name || path.split('/').pop().split(':')[0];
        const role = schema.role || (Object.keys(schema.inputs || {}).length > 0 ? 'method' : 'constructor');
        
        prompt += `### ${role === 'constructor' ? dslName : '.' + dslName}\n`;
        prompt += `- **VFS Path**: \`${path}\`\n`;
        prompt += `- **Role**: \`${role}\`\n`;
        if (schema.description) {
            prompt += `- **Description**: ${schema.description}\n`;
        }
        if (schema.synonyms && schema.synonyms.length > 0) {
            prompt += `- **Synonyms / Alternative Terms**: ${schema.synonyms.join(', ')}\n`;
        }
        
        // Inputs
        if (schema.inputs && Object.keys(schema.inputs).length > 0) {
            prompt += `- **Inputs (Implicit/Chained)**:\n`;
            for (const [name, def] of Object.entries(schema.inputs)) {
                prompt += `  - \`${name}\` (${def.type}): ${def.description || 'Implicitly bound subject.'}\n`;
            }
        }
        
        // Arguments
        if (schema.arguments && schema.arguments.length > 0) {
            prompt += `- **Arguments**:\n`;
            for (const arg of schema.arguments) {
                const optDefault = arg.default !== undefined ? ` (default: ${JSON.stringify(arg.default)})` : '';
                prompt += `  - \`${arg.name}\` (${arg.type})${optDefault}: ${arg.description || ''}\n`;
            }
        }
        
        // Examples
        if (schema.examples && schema.examples.length > 0) {
            prompt += `- **Usage Examples**:\n`;
            for (const ex of schema.examples) {
                prompt += `  \`\`\`js\n  ${ex}\n  \`\`\`\n`;
            }
        }
        prompt += `\n`;
    }
    
    prompt += `## Task:\n`;
    prompt += `Write the JOT script that performs the user's request. Output ONLY valid JOT code inside a markdown code block.\n`;
    
    return prompt;
}
