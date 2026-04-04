import { RESTBridgeBase } from './vfs_rest_bridge_base.js';

/**
 * RESTBridge for Browser environments.
 * Uses native EventSource and fetch.
 */
export class RESTBridge extends RESTBridgeBase {
  constructor(vfs, baseUrl) {
    super(vfs, baseUrl, { 
      EventSource: globalThis.EventSource, 
      fetch: globalThis.fetch.bind(globalThis) 
    });
  }
}
