import RemoteStorage from 'remotestoragejs';

/**
 * RemoteStorage Data Module for JotCAD
 */
export const JotStorageModule = {
  name: 'jotcad',
  builder(privateClient, publicClient) {
    // Define the data structure
    // We use 'private' storage for user operators and blackboard layout
    privateClient.declareType('operator', {
      type: 'object',
      properties: {
        script: { type: 'string' },
        schema: { type: 'object' }
      },
      required: ['script', 'schema']
    });

    privateClient.declareType('layout', {
      type: 'object',
      properties: {
        windows: { type: 'array' },
        desktop: { type: 'array' },
        view: { type: 'object' }
      }
    });

    privateClient.declareType('asset', {
      type: 'object',
      properties: {
        data: { type: 'string' }, // Base64 or URL
        mimeType: { type: 'string' }
      }
    });

    privateClient.declareType('config', {
      type: 'object'
    });

    return {
      exports: {
        // Operator Methods
        getOperators: () => privateClient.getListing('operators/'),
        getOperator: (name) => privateClient.getObject(`operators/${name}`),
        saveOperator: (name, data) => privateClient.storeObject('operator', `operators/${name}`, data),
        removeOperator: (name) => privateClient.remove(`operators/${name}`),

        // Layout Methods
        getLayout: () => privateClient.getObject('layout'),
        saveLayout: (data) => privateClient.storeObject('layout', 'layout', data),

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
