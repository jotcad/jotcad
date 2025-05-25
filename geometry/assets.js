// import { readFile, writeFile } from 'node:fs/promises';

import { FS, cgal } from './getCgal.js';
import { existsSync, mkdirSync } from 'node:fs';

// TODO: Split this out to be node specific.
// Set up local filesystem.

// TODO: Make this configurable.
// Ensure that the target directory exists.
// mkdirSync('/tmp/assets/text', { recursive: true });

let basePath;

export const setBasePath = (path) => {
  basePath = path;
  cgal.SetAssetsBasePath(path);
};

FS.mkdir('assets/text');
setBasePath('assets');

class Assets {
  constructor() {
    this.tag = 'Assets';
    this.text = {};
  }

  getText(id) {
    return FS.readFile(`${basePath}/text/${id}`, { encoding: 'utf8' });
  }

  textId(text) {
    const id = cgal.ComputeTextHash(text);
    const path = `${basePath}/text/${id}`;
    if (basePath === undefined) {
      throw Error('die');
    }
    if (this.text[id]) {
      return id;
    }
    try {
      FS.stat(path);
    } catch (e) {
      if (e.code === 'ENOENT') {
        const result = FS.writeFile(path, text, { encoding: 'utf8' });
      }
    }
    this.text[id] = true;
    return id;
  }
}

export const withAssets = async (op) => {
  const assets = new Assets();
  const result = await op(assets);
  return result;
};
