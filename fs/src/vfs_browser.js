import { VFS as CoreVFS, normalizeSelector } from './vfs_core.js';

export const getCID = async (selector) => {
  const s = normalizeSelector(selector.path, selector.parameters);
  if (!s.path) throw new Error('Selector must have a path');

  const msgUint8 = new TextEncoder().encode(
    s.path + JSON.stringify(s.parameters)
  );
  const hashBuffer = await crypto.subtle.digest('SHA-256', msgUint8);
  const hashArray = Array.from(new Uint8Array(hashBuffer));
  const hashHex = hashArray
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('');
  return hashHex;
};

export class IndexedDBStorage {
  constructor(dbName = 'jotcad-vfs', storeName = 'results') {
    this.dbName = dbName;
    this.storeName = storeName;
    this.db = null;
  }

  async _getDB() {
    if (this.db) return this.db;
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbName, 1);
      request.onupgradeneeded = (e) => {
        const db = e.target.result;
        if (!db.objectStoreNames.contains(this.storeName)) {
          db.createObjectStore(this.storeName);
        }
      };
      request.onsuccess = (e) => {
        this.db = e.target.result;
        resolve(this.db);
      };
      request.onerror = (e) => reject(e.target.error);
    });
  }

  async get(cid) {
    const db = await this._getDB();
    return new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readonly');
      const store = transaction.objectStore(this.storeName);
      const request = store.get(cid);
      request.onsuccess = async () => {
        const entry = request.result;
        if (!entry) return resolve(null);

        // Reconstruct stream from stored Blob or Uint8Array
        const blob =
          entry.data instanceof Blob ? entry.data : new Blob([entry.data]);
        resolve(blob.stream());
      };
      request.onerror = () => reject(request.error);
    });
  }

  async set(cid, stream, info) {
    // For IndexedDB, we must buffer the stream into a Blob
    const reader = stream.getReader();
    const chunks = [];
    while (true) {
      const { done, value } = await reader.read();
      if (done) break;
      chunks.push(value);
    }
    const blob = new Blob(chunks);

    const db = await this._getDB();
    return new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.put({ info, data: blob }, cid);
      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }

  async has(cid) {
    const db = await this._getDB();
    return new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readonly');
      const store = transaction.objectStore(this.storeName);
      const request = store.count(cid);
      request.onsuccess = () => resolve(request.result > 0);
      request.onerror = () => reject(request.error);
    });
  }

  async delete(cid) {
    const db = await this._getDB();
    return new Promise((resolve, reject) => {
      const transaction = db.transaction(this.storeName, 'readwrite');
      const store = transaction.objectStore(this.storeName);
      const request = store.delete(cid);
      request.onsuccess = () => resolve();
      request.onerror = () => reject(request.error);
    });
  }

  async close() {
    if (this.db) {
      this.db.close();
      this.db = null;
    }
  }
}

export class VFS extends CoreVFS {
  constructor(options = {}) {
    super({ getCID, ...options });
  }
}
