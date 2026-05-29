/**
 * Connection: Abstract interface for a direct communication pipe to a neighbor.
 * Honors Identity Duality: Selectors and CIDs are handled via distinct methods
 * to prevent ambiguity and "target guessing."
 */
export class Connection {
  constructor(neighborId) {
    this.neighborId = neighborId;
    this.reachability = 'UNKNOWN';
    this.lastPulse = 0;
    this.pps = 0;
    this._pulseCount = 0;
  }

  _tickPPS() {
    this.pps = this._pulseCount;
    this._pulseCount = 0;
  }

  /**
   * Targets a computational-recipe (Selector).
   */
  async readSelector(selector, context) { throw new Error('Not implemented'); }

  /**
   * Targets a content-hash (CID).
   */
  async readCID(cid, context) { throw new Error('Not implemented'); }

  async spy(selector, context) { throw new Error('Not implemented'); }
  async subscribe(selector, expiresAt, stack) { throw new Error('Not implemented'); }
  async notify(selector, payload, stack) { throw new Error('Not implemented'); }
}
