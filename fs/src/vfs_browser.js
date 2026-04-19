import { VFS as CoreVFS, normalizeSelector } from './vfs_core.js';

export const getCID = async (selector) => {
  const s = normalizeSelector(selector.path, selector.parameters);
  if (!s.path) throw new Error('Selector must have a path');

  // Standardize the JSON representation for hashing
  const msgUint8 = new TextEncoder().encode(JSON.stringify(s));
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
    let blob;
    let bytesReceived = 0;
    const expectedSize = info?.size;

    if (stream instanceof Uint8Array || stream instanceof Blob) {
      blob = stream instanceof Blob ? stream : new Blob([stream]);
      bytesReceived = blob.size;
    } else if (typeof stream === 'string') {
      blob = new Blob([stream]);
      bytesReceived = blob.size;
    } else if (stream && typeof stream.getReader === 'function') {
      // Buffer the stream into a Blob
      const reader = stream.getReader();
      const chunks = [];
      try {
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          chunks.push(value);
          bytesReceived += value.length;
        }
        blob = new Blob(chunks);
      } finally {
        reader.releaseLock();
      }
    } else {
      // Fallback for raw data wrapped in an object or null
      blob = new Blob([stream]);
      bytesReceived = blob.size;
    }

    // Size Verification
    if (expectedSize !== undefined && bytesReceived !== expectedSize) {
      throw new Error(
        `Browser storage write aborted: Expected ${expectedSize} bytes, got ${bytesReceived}`
      );
    }

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

  async *iterateMeta() {
    const db = await this._getDB();
    const transaction = db.transaction(this.storeName, 'readonly');
    const store = transaction.objectStore(this.storeName);

    const request = store.openCursor();
    const results = await new Promise((resolve, reject) => {
      const items = [];
      request.onsuccess = (e) => {
        const cursor = e.target.result;
        if (cursor) {
          items.push({ cid: cursor.key, info: cursor.value.info });
          cursor.continue();
        } else {
          resolve(items);
        }
      };
      request.onerror = () => reject(request.error);
    });

    for (const item of results) {
      yield item;
    }
  }
}

export class VFS extends CoreVFS {
  constructor(options = {}) {
    super({ getCID, ...options });
    this.initialized = false;
  }

  async init() {
    if (this.initialized) return;
    this.initialized = true;

    if (this.storage instanceof IndexedDBStorage) {
      console.log(
        `[VFS ${this.id}] EPHEMERAL WIPE: Cleaning IndexedDB storage...`
      );
      const db = await this.storage._getDB();
      return new Promise((resolve, reject) => {
        const transaction = db.transaction(this.storage.storeName, 'readwrite');
        const store = transaction.objectStore(this.storage.storeName);
        const request = store.clear();
        request.onsuccess = () => {
          console.log(`[VFS ${this.id}] Successfully wiped browser storage.`);
          resolve();
        };
        request.onerror = () => reject(request.error);
      });
    }
  }
}
