import { Selector } from '../../../../fs/src/vfs_browser.js';
import { setSchemas, setDynamicOps } from '../state/MeshState.js';
import { Worksheet } from './Worksheet';

export const JotRegistry = {
  publishDynamicOp(vfs, mesh, path, schema, script, persist = true) {
    const bb = window.blackboard;
    console.log(`[JotRegistry] Publishing: ${path}`);

    if (schema.arguments && !Array.isArray(schema.arguments)) {
      throw new Error(`Catalog Error: Arguments for operator '${path}' must be an array.`);
    }

    vfs.registerProvider(path, async (v, s) => {
      console.log(`[JotRegistry] Executing User Op Script: ${path}`);
      const { JotCompiler } = await import('../../../../jot/src/compiler');
      const { JotParser } = await import('../../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = bb ? bb.schemas() : {};
      for (const [p, sch] of Object.entries(currentSchemas)) {
        const name = p.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path: p, schema: sch });
      }

      // --- MESH BINDING LOGIC ---
      // 1. Identify Subject argument from schema
      const subjectArg = (schema.arguments || []).find(arg => 
        arg.name === '$in' || arg.affiliate === '$out' || arg.affiliate === '$in'
      );
      
      const params = { ...s.parameters };
      
      // 2. Bind Mesh Subject to the identified symbol
      if (s.subject && subjectArg) {
          params[subjectArg.name] = s.subject;
      }

      console.log(`[JotRegistry] Evaluating script for ${path} with params:`, params);

      const parser = new JotParser();
      const ast = parser.parse(script);
      
      // 3. Evaluate with Schema-Driven Extraction
      const results = await compiler.evaluate(ast, params, schema);
      
      // 4. Return the requested output port (defaulting to $out)
      const requestedPort = s.output || '$out';
      const outputBundle = results.find(b => b.selector.output === requestedPort) || results[0];
      const outputSel = outputBundle?.selector;

      if (!outputSel) throw new Error(`Mesh Error: No output terminal found for port '${requestedPort}'`);

      const shapeData = await v.readData(outputSel);
      return new TextEncoder().encode(JSON.stringify(shapeData));
    }, { schema });

    const schemaWithOrigin = { ...schema, _origin: vfs.id };
    setSchemas(prev => ({ ...prev, [path]: schemaWithOrigin }));
    vfs.addSchema(path, schemaWithOrigin);

    setDynamicOps(prev => {
      const next = { ...prev, [path]: { schema, script } };
      if (persist) {
        Worksheet.save(Worksheet.TIERS.OPERATORS, null, next);
      }
      return next;
    });

    if (mesh && mesh.connected) {
        mesh.notify(new Selector('sys/schema'), {
          type: 'CATALOG_ANNOUNCEMENT',
          provider: vfs.id,
          catalog: { [path]: schemaWithOrigin }
        });
    }
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
