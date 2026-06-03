import { Selector } from '../../../../fs/src/vfs_browser.js';
import { MATERIAL_PALETTE } from '../render/AssetManager.js';

/**
 * TextureProvider: Fulfills 'jot/texture' requests.
 * Maps logical material names (e.g., 'steel') to textures (URLs or CIDs).
 */
export const registerTextureProvider = (vfs, mesh) => {
  const schema = {
    path: 'jot/texture',
    description: 'Resolves logical material names to texture data (PNG/JPG).',
    arguments: [
      { name: 'material', type: 'jot:string', description: 'The material name to resolve.' }
    ],
    outputs: {
      '$out': { type: 'jot:bytes', description: 'The raw image bytes or a link to a CID.' }
    }
  };

  vfs.registerProvider('jot/texture', async (v, s) => {
    const name = s.parameters.material;
    if (!name) throw new Error('jot/texture: missing "material" parameter.');

    const resolved = MATERIAL_PALETTE[name];
    if (!resolved) {
      console.warn(`[TextureProvider] Unknown material: ${name}`);
      return null;
    }

    // If it's a URL, fetch it and return the bytes
    if (resolved.startsWith('http') || resolved.startsWith('/')) {
      const resp = await fetch(resolved);
      if (!resp.ok) throw new Error(`jot/texture: Failed to fetch ${resolved}`);
      const arrayBuffer = await resp.arrayBuffer();
      return new Uint8Array(arrayBuffer);
    }

    // If it's a CID, return it as a link (the VFS will handle the redirection)
    return Selector.fromObject({ cid: resolved });
  }, { schema });

  vfs.addSchema('jot/texture', schema);
  
  if (mesh && mesh.connected) {
    mesh.notify(new Selector('sys/schema'), {
      provider: vfs.id,
      catalog: { 'jot/texture': schema }
    });
  }
};
