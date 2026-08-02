import fs from 'node:fs';
import path from 'node:path';

export class UGCStorage {
  constructor(vfs, storagePath = null) {
    this.vfs = vfs;
    this.storagePath = storagePath;
    
    if (!this.storagePath && typeof window === 'undefined') {
      this.storagePath = path.join(process.cwd(), '.ugc_registry.json');
    }
  }

  /**
   * Save a user-defined operator to storage
   * @param {string} name 
   * @param {Object} schema 
   * @param {string} script 
   */
  async save(name, schema, script) {
    const data = await this.readAll();
    data[name] = { schema, script };
    await this.writeAll(data);
  }

  /**
   * Reads all stored operators
   * @returns {Promise<Object>} Operator dictionary
   */
  async readAll() {
    if (typeof window !== 'undefined') {
      const raw = localStorage.getItem('jotcad_ugc_registry');
      return raw ? JSON.parse(raw) : {};
    }

    if (this.storagePath && fs.existsSync(this.storagePath)) {
      try {
        const raw = fs.readFileSync(this.storagePath, 'utf-8');
        return JSON.parse(raw);
      } catch (e) {
        console.error(`[UGCStorage] Error reading registry file:`, e);
      }
    }
    return {};
  }

  /**
   * Writes the full operator dictionary to storage
   * @param {Object} data 
   */
  async writeAll(data) {
    if (typeof window !== 'undefined') {
      localStorage.setItem('jotcad_ugc_registry', JSON.stringify(data, null, 2));
      return;
    }

    if (this.storagePath) {
      try {
        fs.writeFileSync(this.storagePath, JSON.stringify(data, null, 2));
      } catch (e) {
        console.error(`[UGCStorage] Error writing registry file:`, e);
      }
    }
  }
}
