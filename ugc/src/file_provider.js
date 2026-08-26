import fs from 'node:fs';
import path from 'node:path';

export const FILE_SCHEMA = {
  path: 'jot/File',
  description: 'Resolves a local or model file path to its raw bytes.',
  arguments: [
    { name: 'path', type: 'jot:string', description: 'Relative or absolute file path.' }
  ],
  outputs: {
    '$out': { type: 'jot:file', description: 'The raw file bytes stream.' }
  }
};

/**
 * Registers the headless VFS provider for 'jot/File'
 * Sandboxed strictly to the 'ugc/models' directory.
 */
export function registerUGCFileProvider(vfs, mesh = null, modelsRoot = null) {
  const rootDir = path.resolve(modelsRoot || path.join(process.cwd(), 'ugc/models'));

  vfs.registerProvider('jot/File', async (v, s) => {
    const rawPath = s.parameters.path;
    if (!rawPath) throw new Error('jot/File: missing "path" parameter.');

    // 1. Resolve relative to ugc/models root
    const resolvedPath = path.resolve(rootDir, rawPath);

    // 2. Strict sandbox containment check
    if (!resolvedPath.startsWith(rootDir + path.sep) && resolvedPath !== rootDir) {
      throw new Error(`jot/File: Access denied: Path '${rawPath}' traverses outside sandboxed models directory.`);
    }

    if (!fs.existsSync(resolvedPath) || !fs.statSync(resolvedPath).isFile()) {
      throw new Error(`jot/File: Model asset not found '${rawPath}' in models library.`);
    }

    const bytes = fs.readFileSync(resolvedPath);
    const baseName = path.basename(resolvedPath);

    const stream = new ReadableStream({
      start(controller) {
        controller.enqueue(bytes);
        controller.close();
      }
    });

    return {
      stream,
      metadata: {
        state: 'AVAILABLE',
        encoding: 'bytes',
        selector: s.toJSON ? s.toJSON() : s,
        filename: baseName
      }
    };
  }, { schema: FILE_SCHEMA });

  vfs.addSchema('jot/File', FILE_SCHEMA);

  if (mesh && mesh.publishCatalog) {
    mesh.publishCatalog({ 'jot/File': FILE_SCHEMA });
  }
}
