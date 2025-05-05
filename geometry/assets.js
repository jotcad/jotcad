import { cgal } from './getCgal.js';

export const withAssets = async (op) => {
  const assets = {
    text: {}
  };
  const result = await op(assets);
  return result;
};

export const textId = (assets, text) => {
  const hash = cgal.ComputeTextHash(text);
  if (assets.text[hash] === undefined) {
    assets.text[hash] = text;
  }
  return hash;
};
