/**
 * Simple EventEmitter shim for both Node and Browser.
 */
export class EventEmitter {
  constructor() { this.listeners = new Map(); }
  on(event, cb) {
    if (!this.listeners.has(event)) this.listeners.set(event, []);
    this.listeners.get(event).push(cb);
  }
  off(event, cb) {
    const list = this.listeners.get(event);
    if (!list) return;
    const idx = list.indexOf(cb);
    if (idx !== -1) list.splice(idx, 1);
  }
  emit(event, ...args) {
    const list = this.listeners.get(event);
    if (list) list.forEach((cb) => cb(...args));
  }
}
