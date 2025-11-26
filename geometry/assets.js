import { FS, cgal } from './getCgal.js';

// Removed: import { existsSync, mkdirSync } from 'node:fs';

export class Assets {
  constructor(basePath) {
    this.basePath = basePath;
    this.tag = 'Assets';
    this.text = {};
    // Ensure subdirectories exist within the provided basePath
    FS.mkdir(`${basePath}/text`, { recursive: true });
    FS.mkdir(`${basePath}/result`, { recursive: true });
  }

  pathTo(filename) {
    return FS.joinPaths(this.basePath, filename);
  }

  getText(id) {
    const path = `${this.basePath}/text/${id}`;
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
    return FS.writeFile(
      `${this.basePath}/result/${id}`,
      JSON.stringify(result),
      {
        encoding: 'utf8',
      }
    );
  }

  textId(text) {
    const id = cgal.ComputeTextHash(text);
    const path = `${this.basePath}/text/${id}`;
    if (this.basePath === undefined) {
      throw Error('Current session does not have assets path.');
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
