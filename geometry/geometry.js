// import { computeHash } from '@jotcad/sys';
import { cgal } from './getCgal.js';

export let geometries = null;

export const beginGeometry = () => {
  geometries = {};
};

export const endGeometry = () => {
  geometries = null;
};

export const withGeometry = (op) => {
  beginGeometry();
  const result = op();
  endGeometry();
  return result;
};

export const getId = (geometry) => {
  const hash = cgal.ComputeGeometryHash(geometry);
  if (geometries[hash] === undefined) {
    geometries[hash] = geometry;
  }
  return hash;
};

export const fromId = (id) => geometries[id];
