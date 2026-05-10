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
      const { JotCompiler } = await import('../../../../jot/src/compiler');
      const { JotParser } = await import('../../../../jot/src/parser');
      
      const compiler = new JotCompiler(v);
      const currentSchemas = bb ? bb.schemas() : {};
      for (const [p, sch] of Object.entries(currentSchemas)) {
        const name = p.replace(/^(jot|user)\//, '');
        compiler.registerOperator(name, { path: p, schema: sch });
      }

      const parser = new JotParser();
      const ast = parser.parse(script);
      
      const params = { ...s.parameters };
      if (schema.arguments && Array.isArray(schema.arguments)) {
        for (const arg of schema.arguments) {
           if (params[arg.name] === undefined && arg.default !== undefined) {
             params[arg.name] = arg.default;
           }
        }
      }

      const result = await compiler.evaluate(ast, params);
      const primary = Array.isArray(result) ? result[0] : result;
      const shapeData = await v.readData(primary);
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
  }
};
