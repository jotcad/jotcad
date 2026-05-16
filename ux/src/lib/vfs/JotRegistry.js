console.log('[Trace] Executing JotRegistry.js');
import { Selector } from '../../../../fs/src/vfs_browser.js';
import { setSchemas, setDynamicOps } from '../state/MeshState.js';
import { Worksheet } from './Worksheet';

const versions = new Map();
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
        }
    }
    if (Object.keys(current).length > 0) initialized = true;
};

export const JotRegistry = {
  normalizePath(name) {
    if (!name) return '';
    initializeVersionsFromState();
    
    let cleanName = name.split(':v')[0];
    
    // Ensure user/ prefix
    if (!cleanName.startsWith('jot/') && !cleanName.startsWith('user/')) {
        cleanName = `user/${cleanName}`;
    }
    
    const baseName = cleanName.replace(/^(jot|user)\//, '');
    
    // Parse version if present
    const vMatch = name.match(/:v(\d+)$/);
    let version = vMatch ? parseInt(vMatch[1]) : 0;

    // If no version specified, pick the NEXT one
    if (version === 0) {
        const currentV = versions.get(baseName) || 0;
        version = currentV + 1;
    }

    return `${cleanName.startsWith('jot/') ? 'jot' : 'user'}/${baseName}:v${version}`;
  },

  /**
   * Registers or updates a dynamic operator.
   * Does NOT increment versions. Respects the version in the name.
   */
  publishDynamicOp(vfs, mesh, name, schema, script, persist = true) {
    const bb = window.blackboard;
    const targetPath = this.normalizePath(name);
    const [pathPart, vStr] = targetPath.split(':v');
    const cleanName = pathPart.replace('user/', '');
    const version = parseInt(vStr);

    // 1. COLLISION DETECTION
    const existing = bb ? bb.dynamicOps()[targetPath] : null;
    if (existing) {
        const existingScript = (existing.script || '').trim();
        const newScript = (script || '').trim();
        if (existingScript !== newScript) {
            throw new Error(`COLLISION: Version ${targetPath} already exists on the mesh with different code. Please increment version.`);
        }
        console.log(`[JotRegistry] Already published: ${targetPath}`);
        return targetPath;
    }

    console.log(`[JotRegistry] Registering: ${targetPath} (persist=${persist})`);

    if (!schema.arguments || !Array.isArray(schema.arguments)) {
       const msg = `CRITICAL REGISTRY VIOLATION: Attempted to publish operator '${targetPath}' with a malformed (thin) schema.`;
       console.error(msg, schema);
       throw new Error(msg);
    }

    // 2. Register Provider
    vfs.registerProvider(targetPath, async (v, s) => {
      console.log(`[JotRegistry] Executing User Op: ${targetPath}`);
      const { JotCompiler } = await import('../../../../jot/src/compiler');
      const { JotParser } = await import('../../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = bb ? bb.schemas() : {};
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
      
      const result = await compiler.evaluate(ast, params, schema, targetPath);
      
      const requestedPort = s.output || '$out';
      let requestedTerminal = null;

      // 1. Eager Fulfillment
      for (const bundle of result) {
          const port = bundle.port;
          const terminal = bundle.selector;
          if (port === requestedPort) {
              requestedTerminal = terminal;
          } else {
              if (terminal instanceof Selector) {
                  await v.link(s.withOutput(port), terminal);
              } else {
                  await v.write(s.withOutput(port), terminal);
              }
          }
      }

      if (requestedTerminal === null) {
          throw new Error(`Mesh Error: No output terminal found for port '${requestedPort}' in ${targetPath}`);
      }

      return requestedTerminal;
    }, { schema });

    const schemaWithOrigin = { ...schema, _origin: vfs.id, path: targetPath };
    
    // SolidJS updates
    console.log(`[JotRegistry] Updating state for ${targetPath}`);
    setSchemas(prev => ({ ...prev, [targetPath]: schemaWithOrigin }));
    vfs.addSchema(targetPath, schemaWithOrigin);

    setDynamicOps(prev => {
      const next = { ...prev, [targetPath]: { schema, script } };
      if (persist) {
        Worksheet.save(Worksheet.TIERS.OPERATORS, null, next);
      }
      return next;
    });

    if (mesh && mesh.connected) {
        mesh.notify(new Selector('sys/schema'), {
          type: 'CATALOG_ANNOUNCEMENT',
          provider: vfs.id,
          catalog: { [targetPath]: schemaWithOrigin }
        });
    }

    // Update local version tracker
    if (!versions.has(cleanName) || versions.get(cleanName) < version) {
        versions.set(cleanName, version);
    }

    return targetPath;
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
