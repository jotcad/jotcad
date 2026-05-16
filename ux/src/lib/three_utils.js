export * from './render/SharedRenderer.js';
export * from './render/GeometryDecoder.js';
export * from './render/Thumbnailer.js';
export * from './render/AssetManager.js';

// Legacy dummy exports to appease Vite scanner for old test files
export const createScene = () => ({});
export const updateViewports = () => {};

