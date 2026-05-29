import { isSelector } from '../cid.js';

export function validateSelector(schemas, selector) {
    if (typeof selector === 'string' && /^[0-9a-f]{64}$/i.test(selector)) return;
    if (!selector || typeof selector !== 'object') throw new Error('Invalid selector object');
    
    // Parity with C++: path is required and must not be empty
    if (!selector.path || typeof selector.path !== 'string') {
        throw new Error('Schema Violation: Selector path cannot be empty.');
    }

    // Parity with C++: parameters must be an object and cannot be null
    if (selector.parameters === null || typeof selector.parameters !== 'object') {
        throw new Error('Schema Violation: Selector parameters must be an object.');
    }
    
    const schema = schemas.get(selector.path);
    if (schema) {
      const params = selector.parameters || {};

      // 1. Support new array-based arguments format
      if (Array.isArray(schema.arguments)) {
          for (const argDef of schema.arguments) {
              const val = params[argDef.name];
              const isMissing = val === undefined || val === null;
              
              if (isMissing && argDef.default === undefined && !argDef.optional) {
                  const isAffiliate = argDef.affiliate === '$in' || argDef.affiliate === '$out';
                  if (!isAffiliate) {
                      throw new Error(`Missing required parameter '${argDef.name}'`);
                  }
              }

              if (!isMissing) {
                  if (argDef.type === 'jot:number' && typeof val !== 'number') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected number, got ${typeof val}`);
                  }
                  if (argDef.type === 'jot:string' && typeof val !== 'string') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected string, got ${typeof val}`);
                  }
                  if (argDef.type === 'jot:boolean' && typeof val !== 'boolean') {
                      throw new Error(`Invalid parameter type for '${argDef.name}': expected boolean, got ${typeof val}`);
                  }
              }
          }
      }

      // 2. Support legacy JSON-schema style (used in some tests)
      if (schema.required) {
        for (const req of schema.required) {
            if (params[req] === undefined) {
                throw new Error(`Missing required parameter '${req}'`);
            }
        }
      }
      if (schema.properties) {
        for (const [name, def] of Object.entries(schema.properties)) {
          if (params[name] !== undefined) {
            if (def.type === 'number' && typeof params[name] !== 'number') {
              throw new Error(`Invalid parameter type for '${name}'`);
            }
            if (def.enum && !def.enum.includes(params[name])) {
              throw new Error(`Invalid enum value for '${name}'`);
            }
          }
        }
      }
    }
}
