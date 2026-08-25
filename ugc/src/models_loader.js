import fs from 'node:fs';
import path from 'node:path';
import { JotParser } from '../../jot/src/parser.js';

/**
 * Recursively scans a directory for .jot model files and registers them into UserFulfillerManager
 * @param {UserFulfillerManager} fulfillerManager 
 * @param {string} [modelsDir] Base directory to scan
 */
export async function loadModelsFromDirectory(fulfillerManager, modelsDir = null) {
  const baseDir = modelsDir ? path.resolve(modelsDir) : path.resolve(path.dirname(new URL(import.meta.url).pathname), '../models');
  if (!fs.existsSync(baseDir)) {
    return 0;
  }

  const parser = new JotParser();
  const jotFiles = findJotFiles(baseDir);
  let count = 0;

  for (const filePath of jotFiles) {
    const relPath = path.relative(baseDir, filePath).replace(/\.jot$/, '').replace(/\\/g, '/');
    const script = fs.readFileSync(filePath, 'utf-8');
    
    try {
      const ast = parser.parse(script);
      let schema = {
        arguments: [],
        outputs: { $out: { type: 'jot:shape' } }
      };

      if (ast?.type === 'OPERATOR_DEF' && Array.isArray(ast.params)) {
        schema.arguments = ast.params.map(p => ({
          name: p.name,
          type: typeof p.default === 'number' ? 'jot:number' : 'jot:any',
          default: p.default
        }));
      }

      await fulfillerManager.registerFulfiller(relPath, schema, script, false);
      count++;
    } catch (err) {
      console.error(`[ModelsLoader] Failed to load model recipe '${relPath}':`, err.message);
    }
  }

  return count;
}

function findJotFiles(dir) {
  let results = [];
  const entries = fs.readdirSync(dir, { withFileTypes: true });
  for (const entry of entries) {
    const fullPath = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      results = results.concat(findJotFiles(fullPath));
    } else if (entry.isFile() && entry.name.endsWith('.jot')) {
      results.push(fullPath);
    }
  }
  return results;
}
