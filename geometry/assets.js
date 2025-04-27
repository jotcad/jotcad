// import { computeHash } from '@jotcad/sys';
import { cgal } from './getCgal.js';

export let assets = null;

export const beginAssets = () => {
  assets = {
    text: {}
  };
};

export const endAssets = () => {
  assets = null;
};

export const withAssets = (op) => {
  beginAssets();
  const result = op();
  endAssets();
  return result;
};

export const textId = (text) => {
  const hash = cgal.ComputeTextHash(text);
  if (assets.text[hash] === undefined) {
    assets.text[hash] = text;
  }
  return hash;
};
