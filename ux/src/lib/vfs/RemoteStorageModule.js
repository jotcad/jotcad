import RemoteStorage from 'remotestoragejs';

/**
 * RemoteStorage Data Module for JotCAD
 */
export const JotStorageModule = {
  name: 'jotcad',
  builder(privateClient, publicClient) {
    // Define the data structure
    // We use 'private' storage for user operators and blackboard layout
    
    privateClient.declareType('userops', {
      type: 'object'
    });

    privateClient.declareType('layout', {
      type: 'object'
    });

    privateClient.declareType('asset', {
      // Permissive schema to allow strings (blobs) or objects
    });

    privateClient.declareType('config', {
      // Permissive schema
    });

    return {
      exports: {
        // UserOps Methods (Unified dictionary)
        getUserOps: () => privateClient.getObject('userops'),
        saveUserOps: (data) => {
            if (typeof data !== 'object' || data === null) {
                console.error('[RemoteStorageModule] saveUserOps expected object, got:', typeof data, data);
                throw new Error(`Invalid UserOps data: expected object, got ${typeof data}`);
            }
            // Ensure plain object (strips proxies)
            const plainData = JSON.parse(JSON.stringify(data));
            return privateClient.storeObject('userops', 'userops', plainData);
        },

        // Layout Methods
        getLayout: () => privateClient.getObject('layout'),
        saveLayout: (data) => {
            if (typeof data !== 'object' || data === null) {
                console.error('[RemoteStorageModule] saveLayout expected object, got:', typeof data, data);
                throw new Error(`Invalid Layout data: expected object, got ${typeof data}`);
            }
            // Ensure plain object (strips proxies)
            const plainData = JSON.parse(JSON.stringify(data));
            return privateClient.storeObject('layout', 'layout', plainData);
        },

        // Asset Methods
        getAsset: (id) => privateClient.getObject(`assets/${id}`),
        saveAsset: (id, data) => privateClient.storeObject('asset', `assets/${id}`, data),

        // Config/AppData Methods
        getConfig: (tier, key) => privateClient.getObject(`config/${tier}/${key}`),
        saveConfig: (tier, key, data) => privateClient.storeObject('config', `config/${tier}/${key}`, data),

        // Events
        onChange: (callback) => privateClient.on('change', callback),
      }
    };
  }
};

export default JotStorageModule;
