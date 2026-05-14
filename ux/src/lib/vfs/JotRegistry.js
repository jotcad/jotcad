console.log('[Trace] Executing JotRegistry.js');
import { Selector } from '../../../../fs/src/vfs_browser.js';
import { setSchemas, setDynamicOps } from '../state/MeshState.js';
import { Worksheet } from './Worksheet';

const versions = new Map();
const activePaths = new Map();

let initialized = false;

const initializeVersionsFromState = () => {
    if (initialized) return;
    const bb = window.blackboard;
    if (!bb) return;
    const current = bb.schemas();
    for (const path of Object.keys(current)) {
        if (!path.startsWith('user/')) continue;
        const [base, vStr] = path.split(':v');
        const cleanName = base.replace('user/', '');
        const v = vStr ? parseInt(vStr) : 0;
        if (!versions.has(cleanName) || versions.get(cleanName) < v) {
            versions.set(cleanName, v);
            activePaths.set(cleanName, path);
        }
    }
    if (Object.keys(current).length > 0) initialized = true;
};

export const JotRegistry = {
  /**
   * Registers or updates a dynamic operator.
   * Does NOT increment versions. Respects the version in the name.
   */
  publishDynamicOp(vfs, mesh, name, schema, script, persist = true) {
    initializeVersionsFromState();

    const bb = window.blackboard;
    
    // Parse name and version (e.g. user/Foot:v3)
    const [pathPart, vStr] = name.split(':v');
    const cleanName = pathPart.replace(/^(jot|user)\//, '');
    const version = vStr ? parseInt(vStr) : 1; // Default to v1 if no version specified
    
    const newPath = `user/${cleanName}:v${version}`;
    activePaths.set(cleanName, newPath);
    
    // Keep track of latest known version for this name
    if (!versions.has(cleanName) || versions.get(cleanName) < version) {
        versions.set(cleanName, version);
    }

    console.log(`[JotRegistry] Registering: ${newPath} (persist=${persist})`);

    if (!schema.arguments || !Array.isArray(schema.arguments)) {
       const msg = `CRITICAL REGISTRY VIOLATION: Attempted to publish operator '${newPath}' with a malformed (thin) schema.`;
       console.error(msg, schema);
       throw new Error(msg);
    }

    // 2. Register Provider
    vfs.registerProvider(newPath, async (v, s) => {
      console.log(`[JotRegistry] Executing User Op: ${newPath}`);
      const { JotCompiler } = await import('../../../../jot/src/compiler');
      const { JotParser } = await import('../../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = bb ? bb.schemas() : {};
      console.log(`[JotRegistry] Populating compiler with ${Object.keys(currentSchemas).length} schemas.`);
      for (const [p, sch] of Object.entries(currentSchemas)) {
        compiler.registerOperator(p, { path: p, schema: sch });
      }

      // --- MESH BINDING LOGIC ---
      const subjectArg = (schema.arguments || []).find(arg => 
        arg.name === '$in' || arg.affiliate === '$out' || arg.affiliate === '$in'
      );
      
      const params = { ...s.parameters };
      if (s.subject && subjectArg) {
          params[subjectArg.name] = s.subject;
      }

      const parser = new JotParser();
      const ast = parser.parse(script);
      
      const result = await compiler.evaluate(ast, params, schema, newPath);
      
      const requestedPort = s.output || '$out';
      let requestedTerminal = null;

      // 1. Eager Fulfillment: satisfies all schema outputs defined by the compiler results
      for (const bundle of result) {
          const port = bundle.port;
          const terminal = bundle.selector;
          
          if (port === requestedPort) {
              requestedTerminal = terminal;
          } else {
              // Fulfill side ports in storage
              if (terminal instanceof Selector) {
                  await v.link(s.withOutput(port), terminal);
              } else {
                  await v.write(s.withOutput(port), terminal);
              }
          }
      }

      if (requestedTerminal === null) {
          throw new Error(`Mesh Error: No output terminal found for port '${requestedPort}' in ${newPath}`);
      }

      // Return the terminal directly. If it's a Selector, vfs.write will save it as a pointer.
      return requestedTerminal;
    }, { schema });

    const schemaWithOrigin = { ...schema, _origin: vfs.id, path: newPath };
    
    // SolidJS updates - these should ideally be batched by the caller
    setSchemas(prev => ({ ...prev, [newPath]: schemaWithOrigin }));
    vfs.addSchema(newPath, schemaWithOrigin);

    setDynamicOps(prev => {
      const next = { ...prev, [newPath]: { schema, script } };
      if (persist) {
        Worksheet.save(Worksheet.TIERS.OPERATORS, null, next);
      }
      return next;
    });

    if (mesh && mesh.connected) {
        mesh.notify(new Selector('sys/schema'), {
          type: 'CATALOG_ANNOUNCEMENT',
          provider: vfs.id,
          catalog: { [newPath]: schemaWithOrigin }
        });
    }

    return newPath;
  },

  /**
   * Specifically for the "Publish New Version" action.
   * Returns a new path with an incremented version number.
   */
  getNextVersionPath(name) {
    initializeVersionsFromState();
    const cleanName = name.split(':v')[0].replace(/^(jot|user)\//, '');
    const currentV = versions.get(cleanName) || 0;
    const nextV = currentV + 1;
    return `user/${cleanName}:v${nextV}`;
  },

  removeDynamicOp(vfs, mesh, path) {
    console.log(`[JotRegistry] Removing: ${path}`);
    setSchemas(prev => {
        const next = { ...prev };
        delete next[path];
        return next;
    });
    setDynamicOps(prev => {
        const next = { ...prev };
        delete next[path];
        Worksheet.save(Worksheet.TIERS.OPERATORS, null, next);
        return next;
    });
  }
};
