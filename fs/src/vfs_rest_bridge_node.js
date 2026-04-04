import { RESTBridgeBase } from './vfs_rest_bridge_base.js';
import { EventSource } from 'eventsource';

/**
 * RESTBridge for Node.js environments.
 * Uses the 'eventsource' npm package.
 */
export class RESTBridge extends RESTBridgeBase {
  constructor(vfs, baseUrl) {
    super(vfs, baseUrl, { EventSource, fetch: globalThis.fetch });
  }
}
