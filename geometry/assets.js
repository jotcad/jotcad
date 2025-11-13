import { FS, cgal } from './getCgal.js';
import { existsSync, mkdirSync } from 'node:fs';

class Assets {
  constructor(basePath) {
    this.basePath = basePath;
    console.log(`[JS Assets] constructor: basePath = ${basePath}`); // Log basePath
    this.tag = 'Assets';
    this.text = {};
    // Ensure subdirectories exist within the provided basePath
    FS.mkdir(`${basePath}/text`, { recursive: true });
    FS.mkdir(`${basePath}/result`, { recursive: true });
  }

  getText(id) {
    const path = `${this.basePath}/text/${id}`;
    console.log(`[JS Assets] getText: basePath = ${this.basePath}, path = ${path}`); // Log basePath and path
    return FS.readFile(path, { encoding: 'utf8' });
  }

  getResult(id) {
    try {
      return JSON.parse(
        FS.readFile(`${this.basePath}/result/${id}`, { encoding: 'utf8' })
      );
    } catch (e) {
      if (e.code === 'ENOENT') {
        return undefined;
      }
      throw e;
    }
  }

  setResult(id, result) {
    return FS.writeFile(`${this.basePath}/result/${id}`, JSON.stringify(result), {
      encoding: 'utf8',
    });
  }

  textId(text) {
    const id = cgal.ComputeTextHash(text);
    const path = `${this.basePath}/text/${id}`;
    console.log(`[JS Assets] textId: basePath = ${this.basePath}, path = ${path}`); // Log basePath and path
    if (this.basePath === undefined) {
      throw Error('die');
    }
    if (this.text[id]) {
      return id;
    }
    try {
      FS.stat(path); // Check if file exists
    } catch (e) {
      if (e.code === 'ENOENT') {
        const result = FS.writeFile(path, text, { encoding: 'utf8' });
      }
    }
    this.text[id] = true;
    return id;
  }
}

export const withAssets = async (basePath, op) => {
  const assets = new Assets(basePath);
  const result = await op(assets);
  return result;
};
