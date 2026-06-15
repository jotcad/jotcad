import * as THREE from 'three';

export const normalizeId = (id) => {
  if (typeof id === 'string') return id;
  if (id && typeof id === 'object' && id.path) {
    const params = id.parameters || {};
    const sortedParams = Object.keys(params).sort().reduce((acc, key) => { acc[key] = params[key]; return acc; }, {});
    return id.path + '?' + JSON.stringify(sortedParams);
  }
  return JSON.stringify(id);
};

import { createSignal } from 'solid-js';
import { Worksheet } from '../vfs/Worksheet.js';

const DEFAULT_MATERIAL_PALETTE = {
  'grid': 'https://threejs.org/examples/textures/uv_grid_opengl.jpg',
  'steel': 'https://threejs.org/examples/textures/terrain/grasslight-big.jpg',
  'wood': 'https://threejs.org/examples/textures/floors/FloorsCheckerboard_S_Diffuse.jpg',
};

const savedPalette = typeof window !== 'undefined' ? Worksheet.get(Worksheet.TIERS.CONFIG, 'material_palette') : null;

export const MATERIAL_PALETTE = {
  ...DEFAULT_MATERIAL_PALETTE,
  ...(savedPalette || {})
};

const [paletteSignal, setPaletteSignal] = createSignal({ ...MATERIAL_PALETTE });
export { paletteSignal };

export const registerMaterial = (name, urlOrCid) => {
  MATERIAL_PALETTE[name] = urlOrCid;
  setPaletteSignal({ ...MATERIAL_PALETTE });
  if (typeof window !== 'undefined') {
    Worksheet.save(Worksheet.TIERS.CONFIG, 'material_palette', MATERIAL_PALETTE);
  }
};

export const unregisterMaterial = (name) => {
  delete MATERIAL_PALETTE[name];
  setPaletteSignal({ ...MATERIAL_PALETTE });
  if (typeof window !== 'undefined') {
    Worksheet.save(Worksheet.TIERS.CONFIG, 'material_palette', MATERIAL_PALETTE);
  }
};

export class JOTAssets {
  constructor(vfs) { this.vfs = vfs; this.cache = new Map(); this.main = null; }
  async parse(text) {
    let offset = 0; if (text[0] === '\n') offset++;
    while (offset < text.length) {
      if (text[offset] !== '=') break;
      offset++;
      const nl = text.indexOf('\n', offset), h = text.substring(offset, nl);
      offset = nl + 1;
      const sp = h.indexOf(' '), len = parseInt(h.substring(0, sp), 10), name = h.substring(sp + 1);
      const content = text.substring(offset, offset + len);
      offset += len; if (text[offset] === '\n') offset++;
      if (name.startsWith('files/')) this.main = JSON.parse(content);
      else if (name.startsWith('assets/text/')) this.cache.set(name.substring(12), content);
    }
    return this.main;
  }
  async getText(id) {
    const cacheKey = normalizeId(id);
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);
    if (!this.vfs) return null;
    try {
      const data = await blackboard.readCIDData(id);
      if (data) {
        const text = typeof data === 'string' ? data : new TextDecoder().decode(data);
        this.cache.set(cacheKey, text);
        return text;
      }
    } catch (e) { console.warn('[JOTAssets] Resolution failed:', id, e); }
    return null;
  }
  async getTexture(id) {
    const resolvedId = MATERIAL_PALETTE[id] || id;
    const cacheKey = normalizeId(resolvedId) + ':texture';
    if (this.cache.has(cacheKey)) return this.cache.get(cacheKey);

    // Support direct Web URLs
    if (typeof resolvedId === 'string' && (resolvedId.startsWith('http') || resolvedId.startsWith('/'))) {
      const loader = new THREE.TextureLoader();
      try {
        const texture = await new Promise((resolve, reject) => {
          loader.load(resolvedId, resolve, undefined, reject);
        });
        this.cache.set(cacheKey, texture);
        return texture;
      } catch (e) {
        console.warn('[JOTAssets] External texture load failed:', resolvedId, e);
        return null;
      }
    }

    if (!this.vfs) return null;
    try {
      const data = await blackboard.readCIDData(id);
      if (data) {
        const bytes = data instanceof Uint8Array ? data : new TextEncoder().encode(data);
        const blob = new Blob([bytes]);
        const url = URL.createObjectURL(blob);
        const loader = new THREE.TextureLoader();
        const texture = await new Promise((resolve, reject) => {
          loader.load(url, resolve, undefined, reject);
        });
        URL.revokeObjectURL(url);
        this.cache.set(cacheKey, texture);
        return texture;
      }
    } catch (e) { console.warn('[JOTAssets] Texture resolution failed:', id, e); }
    return null;
  }
}

const DEFAULT_SKU_CATALOG = {
  'plywood_3/4': { description: '3/4 inch Birch Plywood', price: '45.00', supplier: 'HomeDepot' },
  'm3_bolt': { description: 'M3 x 10mm Hex Socket Cap Screw', price: '0.15', supplier: 'McMaster' },
};

const savedCatalog = typeof window !== 'undefined' ? Worksheet.get(Worksheet.TIERS.CONFIG, 'sku_catalog') : null;

export const SKU_CATALOG = {
  ...DEFAULT_SKU_CATALOG,
  ...(savedCatalog || {})
};

const [skuCatalogSignal, setSkuCatalogSignal] = createSignal({ ...SKU_CATALOG });
export { skuCatalogSignal };

export const registerSku = (sku, metadata) => {
  SKU_CATALOG[sku] = metadata;
  setSkuCatalogSignal({ ...SKU_CATALOG });
  if (typeof window !== 'undefined') {
    Worksheet.save(Worksheet.TIERS.CONFIG, 'sku_catalog', SKU_CATALOG);
  }
};

export const unregisterSku = (sku) => {
  delete SKU_CATALOG[sku];
  setSkuCatalogSignal({ ...SKU_CATALOG });
  if (typeof window !== 'undefined') {
    Worksheet.save(Worksheet.TIERS.CONFIG, 'sku_catalog', SKU_CATALOG);
  }
};
